#include "integrator.h"
#include "renderer/material.h"
#include "renderer/ray.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <vector>

std::vector<HitpointInfo> gen_eye_path(Scene *scene, Camera *camera,
                                       const Ray &ray, const Point3 &px_pos,
                                       Color *final_value, Random *rnd) {
  std::vector<HitpointInfo> vertices;
  const IntegrateMode mode = IntegrateMode::PT;

  Ray now_ray = ray;
  Point3 lens_pos = now_ray.org; // lens or pinhole position

  Color throughput(1.);
  double pdf = camera->pdf_sample_lens_hole(), pdf_sample_omega;
  Vec3 orienting_normal = camera->sensor_dir,
       orienting_geom_normal = camera->sensor_dir,
       prev_orienting_normal = orienting_normal;

  vertices.emplace_back(HitpointInfo(lens_pos, throughput, pdf, -1,
                                     orienting_normal, orienting_geom_normal,
                                     orienting_normal, ObjectType::LENS));

  HitRecord record;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      *final_value = INVALID_VEC;
      break;
    }

    prev_orienting_normal = orienting_normal;
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
          camera->get_objplane_position(lens_pos, record.hitpoint);
      throughput = throughput *
                   camera->get_bpt_coefficient(px_pos, lens_pos, objplane_pos);
      pdf *= camera->pdf_sample_x1(px_pos, lens_pos, objplane_pos,
                                   record.hitpoint, orienting_normal);
    } else {
      pdf *= pdf_sample_omega * dot(wo, orienting_normal) /
             (now_ray.org - record.hitpoint).len2();
    }

    double g_term = dot(wo, orienting_normal) *
                    dot(now_ray.dir, prev_orienting_normal) /
                    (now_ray.org - record.hitpoint).len2();
    throughput = throughput * g_term;

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() &&
        dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
      vertices.emplace_back(HitpointInfo(
          record.hitpoint, throughput, pdf, record.id, orienting_normal,
          orienting_geom_normal, record.normal, ObjectType::LIGHT));
      *final_value = prim->emission * (throughput / pdf);
      break;
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      *final_value = INVALID_VEC;
      break;
    }
    pdf *= roullete_prob;

    vertices.emplace_back(HitpointInfo(
        record.hitpoint, throughput, pdf, record.id, orienting_normal,
        orienting_geom_normal, record.normal, prim->bxdf->get_objtype()));

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
    if (wi == INVALID_VEC) {
      *final_value = INVALID_VEC;
      break;
    }

    now_ray = dot(wi, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_normal, wi)
                  : Ray(record.hitpoint - EPS * orienting_normal, wi);
    throughput =
        throughput * prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                      orienting_geom_normal, into, mode);
    pdf_sample_omega =
        prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
  }

  return vertices;
}

std::vector<HitpointInfo> gen_light_path(Scene *scene, Camera *camera,
                                         PixelResult *result, Random *rnd) {
  std::vector<HitpointInfo> vertices;
  const IntegrateMode mode = IntegrateMode::LT;

  int32_t light_id = scene->sample_light_uniform(rnd);
  if (light_id < 0) {
    // no light
    result->value = INVALID_VEC;
    return vertices;
  }

  double pdf, pdf_sample_omega;
  Point3 light_pos =
      scene->primitives[light_id].shape->sample_surface(rnd, &pdf);
  Vec3 light_normal = scene->primitives[light_id].shape->get_normal(light_pos);
  pdf *= scene->pdf_sample_light_uniform();
  Color throughput = scene->primitives[light_id].emission;

  vertices.emplace_back(HitpointInfo(light_pos, throughput, pdf, light_id,
                                     light_normal, light_normal, light_normal,
                                     ObjectType::LIGHT));

  Vec3 dir = Bxdf::sample_cosine_hemisphere(light_normal, rnd);
  pdf *= fabs(dot(light_normal, dir)) * PI_INV;
  Ray now_ray = Ray(light_pos + EPS * light_normal, dir);

  Vec3 orienting_normal = light_normal, orienting_geom_normal = light_normal,
       prev_orienting_normal = light_normal;
  HitRecord record;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    bool scene_hit = scene->intersect(now_ray, &record);

    Vec3 wi = -1. * now_ray.dir;
    prev_orienting_normal = orienting_normal;

    Point3 px_pos, lens_pos, objplane_pos;
    uint32_t px_u, px_v;
    if (camera->intersect_lens(now_ray, record.distance, &px_pos, &lens_pos,
                               &objplane_pos, &px_u, &px_v)) {
      double g_term = dot(wi, camera->sensor_dir) *
                      dot(now_ray.dir, prev_orienting_normal) /
                      (lens_pos - now_ray.org).len2();
      throughput = throughput * g_term *
                   camera->get_bpt_coefficient(px_pos, lens_pos, objplane_pos);
      pdf *= pdf_sample_omega * dot(wi, camera->sensor_dir) /
             (lens_pos - now_ray.org).len2();

      vertices.emplace_back(HitpointInfo(lens_pos, throughput, pdf, -1,
                                         camera->sensor_dir, camera->sensor_dir,
                                         camera->sensor_dir, ObjectType::LENS));
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

    pdf *= pdf_sample_omega * dot(wi, orienting_normal) /
           (now_ray.org - record.hitpoint).len2();

    double g_term = dot(wi, orienting_normal) *
                    dot(now_ray.dir, prev_orienting_normal) /
                    (now_ray.org - record.hitpoint).len2();
    throughput = throughput * g_term;

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      result->value = INVALID_VEC;
      break;
    }
    pdf *= roullete_prob;

    Primitive *prim = &scene->primitives[record.id];

    vertices.emplace_back(HitpointInfo(
        record.hitpoint, throughput, pdf, record.id, orienting_normal,
        orienting_geom_normal, record.normal, prim->bxdf->get_objtype()));

    if (prim->is_light() &&
        dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
      result->value = INVALID_VEC;
      break;
    }

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wo = prim->bxdf->sample(wi, orienting_normal, into, rnd, mode);
    if (wo == INVALID_VEC) {
      result->value = INVALID_VEC;
      break;
    }

    now_ray = dot(wo, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_normal, wo)
                  : Ray(record.hitpoint - EPS * orienting_normal, wo);
    throughput =
        throughput * prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                      orienting_geom_normal, into, mode);
    pdf_sample_omega =
        prim->bxdf->sampling_pdf(wi, wo, orienting_normal, into, mode);
  }

  return vertices;
}

double calc_mis_weight() { return 1.; }

std::vector<PixelResult>
Integrator::bpt_integrate(Scene *scene, Camera *camera, const Point3 &px_pos,
                          const uint32_t px_u, const uint32_t px_v,
                          const Ray &ray, Random *rnd) {
  std::vector<PixelResult> bpt_results;

  Color eyepath_final_value;
  std::vector<HitpointInfo> eye_path =
      gen_eye_path(scene, camera, ray, px_pos, &eyepath_final_value, rnd);
  PixelResult eyepath_result =
      PixelResult(px_u, px_v, eyepath_final_value, false);

  PixelResult lightpath_result;
  std::vector<HitpointInfo> light_path =
      gen_light_path(scene, camera, &lightpath_result, rnd);

  if (eyepath_result.value != INVALID_VEC) {
    double mis_weight = calc_mis_weight();
    bpt_results.emplace_back(PixelResult(
        px_u, px_v, mis_weight * eyepath_result.get_value(), false));
  }

  if (lightpath_result.value != INVALID_VEC) {
    double mis_weight = calc_mis_weight();
    bpt_results.emplace_back(lightpath_result.px_u, lightpath_result.px_v,
                             mis_weight * lightpath_result.get_value(), true);
  }

  for (uint32_t en = 1; en <= eye_path.size(); en++) {
    for (uint32_t ln = 1; ln <= light_path.size(); ln++) {
      uint32_t final_px_u = px_u, final_px_v = px_v;
      HitpointInfo *eye_end = &eye_path[en - 1],
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
      HitRecord test_record;
      Vec3 light_end_to_eye_end = eye_end->hitpoint - light_end->hitpoint;
      Vec3 test_dir = normalize(light_end_to_eye_end);
      Vec3 test_org =
          light_end->type == ObjectType::OTHER_TRANSPARENT &&
                  dot(light_end_to_eye_end, light_end->orienting_normal) < 0.
              ? light_end->hitpoint - EPS * light_end->orienting_normal
              : light_end->hitpoint + EPS * light_end->orienting_normal;
      Ray test_ray = Ray(test_org, test_dir);
      scene->intersect(test_ray, &test_record);

      switch (eye_end->type) {
      case ObjectType::OTHER: {
        if ((eye_end->hitpoint - test_record.hitpoint).len() > EPS) {
          continue;
        }
        Vec3 wo = normalize(eye_path[en - 2].hitpoint - eye_end->hitpoint);
        throughput =
            throughput * scene->primitives[eye_end->id].bxdf->eval(
                             wo, -1. * test_dir, test_record.uv,
                             eye_end->orienting_normal,
                             eye_end->geometry_normal, true, IntegrateMode::LT);
      } break;
      case ObjectType::OTHER_TRANSPARENT: {
        if ((eye_end->hitpoint - test_record.hitpoint).len() > EPS) {
          continue;
        }
        Vec3 wo = normalize(eye_path[en - 2].hitpoint - eye_end->hitpoint);
        Vec3 orienting_normal = dot(test_dir, eye_end->obj_normal) < 0.
                                    ? eye_end->obj_normal
                                    : -1. * eye_end->obj_normal;
        Vec3 orienting_geom_normal =
            dot(eye_end->geometry_normal, orienting_normal) > 0.
                ? eye_end->geometry_normal
                : -1. * eye_end->geometry_normal;
        bool into = dot(orienting_normal, eye_end->obj_normal) > 0.;
        throughput = throughput * scene->primitives[eye_end->id].bxdf->eval(
                                      wo, -1. * test_dir, test_record.uv,
                                      orienting_normal, orienting_geom_normal,
                                      into, IntegrateMode::LT);
      } break;
      case ObjectType::LENS: {
        Point3 px_pos, lens_pos, objplane_pos;
        if (camera->intersect_lens(test_ray, test_record.distance, &px_pos,
                                   &lens_pos, &objplane_pos, &final_px_u,
                                   &final_px_v)) {
          throughput = throughput * camera->get_bpt_coefficient(
                                        px_pos, lens_pos, objplane_pos);
        }
      } break;
      default:
        continue;
      }

      switch (light_end->type) {
      case ObjectType::OTHER: {
        Vec3 wi = normalize(light_path[ln - 2].hitpoint - light_end->hitpoint);
        Vec2 uv = scene->primitives[light_end->id].shape->get_texcoord(
            light_end->hitpoint);
        throughput = throughput *
                     scene->primitives[light_end->id].bxdf->eval(
                         test_dir, wi, uv, light_end->orienting_normal,
                         light_end->geometry_normal, true, IntegrateMode::LT);
      } break;
      case ObjectType::OTHER_TRANSPARENT: {
        Vec3 wi = normalize(light_path[ln - 2].hitpoint - light_end->hitpoint);
        Vec2 uv = scene->primitives[light_end->id].shape->get_texcoord(
            light_end->hitpoint);
        Vec3 orienting_normal = dot(test_dir, light_end->obj_normal) < 0.
                                    ? light_end->obj_normal
                                    : -1. * light_end->obj_normal;
        Vec3 orienting_geom_normal =
            dot(light_end->geometry_normal, orienting_normal) > 0.
                ? light_end->geometry_normal
                : -1. * light_end->geometry_normal;
        bool into = dot(orienting_normal, light_end->obj_normal) > 0.;
        throughput =
            throughput * scene->primitives[light_end->id].bxdf->eval(
                             test_dir, wi, uv, orienting_normal,
                             orienting_geom_normal, into, IntegrateMode::LT);
      } break;
      case ObjectType::LIGHT:
        break;
      default:
        continue;
      }

      double g_term =
          fabs(dot(light_end_to_eye_end, eye_end->orienting_normal) *
               dot(light_end_to_eye_end, light_end->orienting_normal)) /
          light_end_to_eye_end.len2();
      double mis_weight = calc_mis_weight();
      if (std::isnan(mis_weight)) {
        continue;
      }

      Color value = throughput * g_term * mis_weight / pdf;
      bpt_results.emplace_back(
          PixelResult(final_px_u, final_px_v, value, en <= 1));
    }
  }

  return bpt_results;
}
