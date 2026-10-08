#include "integrator.h"
#include "renderer/camera.h"
#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/scene.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <vector>

std::vector<BptVertexInfo> gen_eye_path(Scene *scene, Camera *camera,
                                        const Ray &ray, const Point3 &px_pos,
                                        Color *final_value, Random *rnd) {
  std::vector<BptVertexInfo> vertices;
  const IntegrateMode mode = IntegrateMode::PT;

  Ray now_ray = ray;
  Point3 lens_hole_pos = now_ray.org; // lens or pinhole position

  Color throughput(1.);
  double pdf = camera->pdf_sample_lens_hole(), pdf_sample_omega = 1.;
  Vec3 orienting_normal = camera->sensor_dir,
       orienting_geom_normal = camera->sensor_dir,
       prev_orienting_geom_normal = orienting_geom_normal;

  vertices.emplace_back(BptVertexInfo(
      lens_hole_pos, throughput, pdf, -1, orienting_normal,
      orienting_geom_normal, orienting_normal, Vec2(), camera->get_objtype()));

  HitRecord record;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      *final_value = INVALID_VEC;
      break;
    }

    prev_orienting_geom_normal = orienting_geom_normal;
    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;
    orienting_geom_normal = dot(record.geometry_normal, orienting_normal) > 0.
                                ? record.geometry_normal
                                : -1. * record.geometry_normal;

    Vec3 wo = -1. * now_ray.dir;

    if (depth == 0) {
      // lens -> x1
      Point3 objplane_pos =
          camera->get_objplane_position(lens_hole_pos, record.hitpoint);
      throughput = throughput * camera->get_bpt_coefficient(
                                    px_pos, lens_hole_pos, objplane_pos);
      pdf *= camera->pdf_sample_x1(px_pos, lens_hole_pos, objplane_pos,
                                   record.hitpoint, orienting_geom_normal);
    } else {
      pdf *= pdf_sample_omega * dot(wo, orienting_geom_normal) /
             (now_ray.org - record.hitpoint).len2();
    }

    double g_term = fabs(dot(wo, orienting_geom_normal) *
                         dot(now_ray.dir, prev_orienting_geom_normal)) /
                    (now_ray.org - record.hitpoint).len2();
    throughput = throughput * g_term;

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light()) {
      if (dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
        vertices.emplace_back(
            BptVertexInfo(record.hitpoint, throughput, pdf, record.id,
                          orienting_normal, orienting_geom_normal,
                          record.normal, record.uv, ObjectType::LIGHT));
        *final_value = prim->emission * (throughput / pdf);
      } else {
        *final_value = INVALID_VEC;
      }
      break;
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      *final_value = INVALID_VEC;
      break;
    }
    pdf *= roullete_prob;

    vertices.emplace_back(BptVertexInfo(record.hitpoint, throughput, pdf,
                                        record.id, orienting_normal,
                                        orienting_geom_normal, record.normal,
                                        record.uv, prim->bxdf->get_objtype()));

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
    if (wi == INVALID_VEC) {
      *final_value = INVALID_VEC;
      break;
    }

    now_ray = dot(wi, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_geom_normal, wi)
                  : Ray(record.hitpoint - EPS * orienting_geom_normal, wi);
    throughput =
        throughput * prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                      orienting_geom_normal, into, mode);
    pdf_sample_omega =
        prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
  }

  return vertices;
}

std::vector<BptVertexInfo> gen_light_path(Scene *scene, Camera *camera,
                                          PixelResult *result, Random *rnd) {
  std::vector<BptVertexInfo> vertices;
  const IntegrateMode mode = IntegrateMode::LT;

  int32_t light_id = scene->sample_light_uniform(rnd);
  if (light_id < 0) {
    // no light
    result->value = INVALID_VEC;
    return vertices;
  }

  double pdf = 1.;
  Point3 light_pos =
      scene->primitives[light_id].shape->sample_surface(rnd, &pdf);
  Vec3 light_normal = scene->primitives[light_id].shape->get_normal(light_pos);
  Vec2 light_uv = scene->primitives[light_id].shape->get_texcoord(light_pos);
  pdf *= scene->pdf_sample_light_uniform();
  Color throughput = scene->primitives[light_id].emission;

  vertices.emplace_back(BptVertexInfo(light_pos, throughput, pdf, light_id,
                                      light_normal, light_normal, light_normal,
                                      light_uv, ObjectType::LIGHT));

  Vec3 dir = Bxdf::sample_cosine_hemisphere(light_normal, rnd);
  double pdf_sample_omega = fabs(dot(light_normal, dir)) * PI_INV;
  Ray now_ray = Ray(light_pos + EPS * light_normal, dir);

  Vec3 orienting_normal = light_normal, orienting_geom_normal = light_normal,
       prev_orienting_geom_normal = light_normal;
  HitRecord record;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    bool scene_hit = scene->intersect(now_ray, &record);

    Vec3 wi = -1. * now_ray.dir;
    prev_orienting_geom_normal = orienting_geom_normal;

    Point3 px_pos, lens_pos, objplane_pos;
    uint32_t px_u, px_v;
    if (camera->get_objtype() == ObjectType::LENS &&
        camera->intersect_lens_hole(now_ray, record.distance, &px_pos,
                                    &lens_pos, &objplane_pos, &px_u, &px_v)) {
      double g_term = fabs(dot(wi, camera->sensor_dir) *
                           dot(now_ray.dir, prev_orienting_geom_normal)) /
                      (lens_pos - now_ray.org).len2();
      throughput = throughput * g_term *
                   camera->get_bpt_coefficient(px_pos, lens_pos, objplane_pos);
      pdf *= pdf_sample_omega * dot(wi, camera->sensor_dir) /
             (lens_pos - now_ray.org).len2();

      vertices.emplace_back(BptVertexInfo(
          lens_pos, throughput, pdf, -1, camera->sensor_dir, camera->sensor_dir,
          camera->sensor_dir, Vec2(), ObjectType::LENS));
      *result = PixelResult(px_u, px_v, throughput / pdf, true);
      break;
    }

    if (!scene_hit) {
      result->value = INVALID_VEC;
      break;
    }

    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;
    orienting_geom_normal = dot(record.geometry_normal, orienting_normal) > 0.
                                ? record.geometry_normal
                                : -1. * record.geometry_normal;

    pdf *= pdf_sample_omega * dot(wi, orienting_geom_normal) /
           (now_ray.org - record.hitpoint).len2();

    double g_term = fabs(dot(wi, orienting_geom_normal) *
                         dot(now_ray.dir, prev_orienting_geom_normal)) /
                    (now_ray.org - record.hitpoint).len2();
    throughput = throughput * g_term;

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      result->value = INVALID_VEC;
      break;
    }
    pdf *= roullete_prob;

    Primitive *prim = &scene->primitives[record.id];

    if (prim->is_light()) {
      result->value = INVALID_VEC;
      break;
    }

    vertices.emplace_back(BptVertexInfo(record.hitpoint, throughput, pdf,
                                        record.id, orienting_normal,
                                        orienting_geom_normal, record.normal,
                                        record.uv, prim->bxdf->get_objtype()));

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wo = prim->bxdf->sample(wi, orienting_normal, into, rnd, mode);
    if (wo == INVALID_VEC) {
      result->value = INVALID_VEC;
      break;
    }

    now_ray = dot(wo, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_geom_normal, wo)
                  : Ray(record.hitpoint - EPS * orienting_geom_normal, wo);
    throughput =
        throughput *
        prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                         orienting_geom_normal, into, mode) *
        correct_shading_normal(wi, wo, orienting_geom_normal, orienting_normal);
    pdf_sample_omega =
        prim->bxdf->sampling_pdf(wi, wo, orienting_normal, into, mode);
  }

  return vertices;
}

double calc_sampling_pdf(const std::vector<const BptVertexInfo *> &path,
                         Camera *camera, Scene *scene, const uint32_t to_idx,
                         const uint32_t from_idx, const uint32_t prev_from_idx,
                         const IntegrateMode &mode) {
  const BptVertexInfo *to_v = path[to_idx], *from_v = path[from_idx];
  const BptVertexInfo *prev_from_v =
      prev_from_idx < path.size() ? path[prev_from_idx] : nullptr;
  double pdf_omega = 1.;

  Vec3 to = to_v->hitpoint - from_v->hitpoint;
  Vec3 normalized_to = normalize(to);

  switch (from_v->type) {
  case ObjectType::LIGHT: {
    pdf_omega = fmax(dot(from_v->orienting_normal, normalized_to), 0.) * PI_INV;
  } break;
  case ObjectType::OTHER: {
    if (!prev_from_v) {
      break;
    }

    Vec3 dir_prev = normalize(prev_from_v->hitpoint - from_v->hitpoint);
    Vec3 dir_next = normalized_to;
    pdf_omega = scene->primitives[from_v->id].bxdf->sampling_pdf(
        dir_prev, dir_next, from_v->orienting_normal, true, mode);
  } break;
  case ObjectType::FULL_SPECUALR:
  case ObjectType::OTHER_TRANSPARENT: {
    if (!prev_from_v) {
      break;
    }

    Vec3 dir_prev = normalize(prev_from_v->hitpoint - from_v->hitpoint);
    Vec3 dir_next = normalized_to;
    Vec3 orienting_normal = dot(dir_prev, from_v->obj_normal) > 0.
                                ? from_v->obj_normal
                                : -1. * from_v->obj_normal;
    bool into = dot(from_v->obj_normal, orienting_normal) > 0.;
    pdf_omega = scene->primitives[from_v->id].bxdf->sampling_pdf(
        dir_prev, dir_next, orienting_normal, into, mode);
  } break;
  case ObjectType::LENS: {
    Point3 px_pos, lens_pos, objplane_pos;
    uint32_t tmp_u, tmp_v;
    Ray test_ray = Ray(to_v->hitpoint, -1. * normalized_to);
    if (!camera->intersect_lens_hole(test_ray, INF, &px_pos, &lens_pos,
                                     &objplane_pos, &tmp_u, &tmp_v)) {
      return 0.;
    }

    return camera->pdf_sample_x1(px_pos, lens_pos, objplane_pos, to_v->hitpoint,
                                 to_v->geometry_normal);
  } break;
  case ObjectType::PINHOLE: {
    Point3 px_pos, hole_pos, objplane_pos;
    uint32_t tmp_u, tmp_v;
    Ray test_ray = Ray(to_v->hitpoint, -1. * normalized_to);
    if (!camera->intersect_lens_hole(test_ray, INF, &px_pos, &hole_pos,
                                     &objplane_pos, &tmp_u, &tmp_v)) {
      return 0.;
    }

    return camera->pdf_sample_x1(px_pos, hole_pos, objplane_pos, to_v->hitpoint,
                                 to_v->geometry_normal);
  } break;
  case ObjectType::MEDIUM: {
    return 0.;
  } break;
  }

  return pdf_omega * fabs(dot(normalized_to, to_v->geometry_normal)) /
         to.len2();
}

double calc_mis_weight(Scene *scene, Camera *camera,
                       const std::vector<BptVertexInfo> &eye_path,
                       const std::vector<BptVertexInfo> &light_path,
                       const uint32_t en, const uint32_t ln) {
  uint32_t size = en + ln;
  std::vector<const BptVertexInfo *> path(size);
  std::vector<double> pi1_pi(size);

  for (uint32_t i = 0; i < ln; i++) {
    path[i] = &light_path[i];
  }

  for (uint32_t i = 0; i < en; i++) {
    path[ln + i] = &eye_path[en - i - 1];
  }

  pi1_pi[0] =
      scene->pdf_sample_light_uniform() *
      scene->primitives[path[0]->id].shape->pdf_sample_surface_area() /
      calc_sampling_pdf(path, camera, scene, 0, 1, 2, IntegrateMode::PT);

  pi1_pi[size - 1] = calc_sampling_pdf(path, camera, scene, size - 1, size - 2,
                                       size - 3, IntegrateMode::LT) /
                     camera->pdf_sample_lens_hole();

  for (uint32_t i = 1; i < size - 1; i++) {
    double pdf_from_light = calc_sampling_pdf(path, camera, scene, i, i - 1,
                                              i - 2, IntegrateMode::LT);

    double pdf_from_eye = calc_sampling_pdf(path, camera, scene, i, i + 1,
                                            i + 2, IntegrateMode::PT);

    double q_from_light = roullete(i - 1);
    double q_from_eye = roullete(size - 2 - i);

    pi1_pi[i] = (pdf_from_light * q_from_light) / (pdf_from_eye * q_from_eye);
  }

  std::vector<double> p(size + 1);
  // p[ln] = total_pdf;
  p[ln] = 1.;
  for (int32_t i = ln - 1; i >= 0; i--) {
    p[i] = p[i + 1] / pi1_pi[i];
  }

  for (uint32_t i = ln; i < size; i++) {
    p[i + 1] = p[i] * pi1_pi[i];
  }

  for (uint32_t i = 0; i < size; i++) {
    if (path[i]->type == ObjectType::FULL_SPECUALR) {
      p[i] = 0., p[i + 1] = 0.;
    }
  }

  if (camera->get_objtype() == ObjectType::PINHOLE) {
    p[size] = 0.;
  }

  double sum = 0.;
  for (double p_ : p) {
    sum += p_;
  }

  return p[ln] / sum;
}

std::vector<PixelResult>
Integrator::bpt_integrate(Scene *scene, Camera *camera, const Point3 &px_pos,
                          const uint32_t px_u, const uint32_t px_v,
                          const Ray &ray, Random *rnd) {
  std::vector<PixelResult> bpt_results;

  Color eyepath_final_value;
  std::vector<BptVertexInfo> eye_path =
      gen_eye_path(scene, camera, ray, px_pos, &eyepath_final_value, rnd);
  PixelResult eyepath_result =
      PixelResult(px_u, px_v, eyepath_final_value, false);

  PixelResult lightpath_result;
  std::vector<BptVertexInfo> light_path =
      gen_light_path(scene, camera, &lightpath_result, rnd);

  if (eyepath_result.value != INVALID_VEC) {
    double total_pdf = eye_path[eye_path.size() - 1].pdf;
    double mis_weight = calc_mis_weight(scene, camera, eye_path, light_path,
                                        eye_path.size(), 0);
    bpt_results.emplace_back(PixelResult(
        px_u, px_v, mis_weight * eyepath_result.get_value(), false));
  }

  if (lightpath_result.value != INVALID_VEC) {
    double total_pdf = light_path[light_path.size() - 1].pdf;
    double mis_weight = calc_mis_weight(scene, camera, eye_path, light_path, 0,
                                        light_path.size());
    bpt_results.emplace_back(lightpath_result.px_u, lightpath_result.px_v,
                             mis_weight * lightpath_result.get_value(), true);
  }

  Ray test_ray;

  for (uint32_t en = 1; en <= eye_path.size(); en++) {
    for (uint32_t ln = 1; ln <= light_path.size(); ln++) {
      uint32_t final_px_u = px_u, final_px_v = px_v;
      BptVertexInfo *eye_end = &eye_path[en - 1],
                    *light_end = &light_path[ln - 1];

      if (eye_end->type == ObjectType::FULL_SPECUALR ||
          eye_end->type == ObjectType::LIGHT ||
          light_end->type == ObjectType::FULL_SPECUALR ||
          light_end->type == ObjectType::LENS) {
        continue;
      }

      double pdf = eye_end->pdf * light_end->pdf;
      if (pdf <= 0) {
        continue;
      }

      Color throughput = eye_end->throughput * light_end->throughput;
      Vec3 light_end_to_eye_end = eye_end->hitpoint - light_end->hitpoint;
      HitRecord test_record(light_end_to_eye_end.len() - 2 * EPS);
      Vec3 test_dir = normalize(light_end_to_eye_end);
      Vec3 test_org =
          light_end->type == ObjectType::OTHER_TRANSPARENT &&
                  dot(light_end_to_eye_end, light_end->orienting_normal) < 0.
              ? light_end->hitpoint - EPS * light_end->geometry_normal
              : light_end->hitpoint + EPS * light_end->geometry_normal;
      test_ray = Ray(test_org, test_dir);
      if (scene->intersect(test_ray, &test_record)) {
        continue;
      }

      switch (eye_end->type) {
      case ObjectType::OTHER: {
        Vec3 wo = normalize(eye_path[en - 2].hitpoint - eye_end->hitpoint);
        throughput =
            throughput * scene->primitives[eye_end->id].bxdf->eval(
                             wo, -1. * test_dir, eye_end->uv,
                             eye_end->orienting_normal,
                             eye_end->geometry_normal, true, IntegrateMode::PT);
      } break;
      case ObjectType::OTHER_TRANSPARENT: {
        Vec3 wo = normalize(eye_path[en - 2].hitpoint - eye_end->hitpoint);
        Vec3 orienting_normal = dot(test_dir, eye_end->obj_normal) < 0.
                                    ? eye_end->obj_normal
                                    : -1. * eye_end->obj_normal;
        Vec3 orienting_geom_normal =
            dot(eye_end->geometry_normal, orienting_normal) > 0.
                ? eye_end->geometry_normal
                : -1. * eye_end->geometry_normal;
        bool into = dot(orienting_normal, eye_end->obj_normal) > 0.;
        throughput =
            throughput * scene->primitives[eye_end->id].bxdf->eval(
                             wo, -1. * test_dir, eye_end->uv, orienting_normal,
                             orienting_geom_normal, into, IntegrateMode::PT);
      } break;
      case ObjectType::PINHOLE:
      case ObjectType::LENS: {
        Point3 px_pos, lens_pos, objplane_pos;
        if (camera->intersect_lens_hole(test_ray, INF, &px_pos, &lens_pos,
                                        &objplane_pos, &final_px_u,
                                        &final_px_v)) {
          throughput = throughput * camera->get_bpt_coefficient(
                                        px_pos, lens_pos, objplane_pos);
        } else {
          continue;
        }
      } break;
      default:
        continue;
      }

      switch (light_end->type) {
      case ObjectType::OTHER: {
        Vec3 wi = normalize(light_path[ln - 2].hitpoint - light_end->hitpoint);
        throughput =
            throughput *
            scene->primitives[light_end->id].bxdf->eval(
                test_dir, wi, light_end->uv, light_end->orienting_normal,
                light_end->geometry_normal, true, IntegrateMode::LT) *
            correct_shading_normal(wi, test_dir, light_end->geometry_normal,
                                   light_end->orienting_normal);
      } break;
      case ObjectType::OTHER_TRANSPARENT: {
        Vec3 wi = normalize(light_path[ln - 2].hitpoint - light_end->hitpoint);
        Vec3 orienting_normal = dot(test_dir, light_end->obj_normal) < 0.
                                    ? light_end->obj_normal
                                    : -1. * light_end->obj_normal;
        Vec3 orienting_geom_normal =
            dot(light_end->geometry_normal, orienting_normal) > 0.
                ? light_end->geometry_normal
                : -1. * light_end->geometry_normal;
        bool into = dot(orienting_normal, light_end->obj_normal) > 0.;
        throughput =
            throughput *
            scene->primitives[light_end->id].bxdf->eval(
                test_dir, wi, light_end->uv, orienting_normal,
                orienting_geom_normal, into, IntegrateMode::LT) *
            correct_shading_normal(wi, test_dir, light_end->geometry_normal,
                                   light_end->orienting_normal);
      } break;
      case ObjectType::LIGHT:
        if (dot(test_dir, light_end->orienting_normal) < 0.) {
          continue;
        } else {
          break;
        }
      default:
        continue;
      }

      if (throughput.is_lezero()) {
        continue;
      }

      double g_term = fabs(dot(test_dir, eye_end->geometry_normal) *
                           dot(test_dir, light_end->geometry_normal)) /
                      light_end_to_eye_end.len2();
      double mis_weight =
          calc_mis_weight(scene, camera, eye_path, light_path, en, ln);
      if (std::isnan(mis_weight) || mis_weight > INF || mis_weight < 0) {
        continue;
      }

      Color value = throughput * g_term * mis_weight / pdf;
      bpt_results.emplace_back(
          PixelResult(final_px_u, final_px_v, value, en <= 1));
    }
  }

  return bpt_results;
}
