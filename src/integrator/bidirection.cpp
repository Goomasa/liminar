#include "integrator.h"
#include "renderer/material.h"
#include "renderer/ray.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <vector>

std::vector<HitpointInfo> gen_eye_path(Scene *scene, Camera *camera, Ray ray,
                                       const Point3 &px_pos, Color *final_value,
                                       Random *rnd) {
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
                                     ObjectType::LENS));

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
          orienting_geom_normal, ObjectType::LIGHT));
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
        orienting_geom_normal, ObjectType::PRIMITIVE));

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
                                         uint32_t *px_u, uint32_t *px_v,
                                         Color *final_value, Random *rnd) {
  std::vector<HitpointInfo> vertices;
  const IntegrateMode mode = IntegrateMode::LT;

  int32_t light_id = scene->sample_light_uniform(rnd);
  if (light_id < 0) {
    // no light
    *final_value = INVALID_VEC;
    return vertices;
  }

  double pdf, pdf_sample_omega;
  Point3 light_pos =
      scene->primitives[light_id].shape->sample_surface(rnd, &pdf);
  Vec3 light_normal = scene->primitives[light_id].shape->get_normal(light_pos);
  pdf *= scene->pdf_sample_light_uniform();
  Color throughput = scene->primitives[light_id].emission;

  vertices.emplace_back(HitpointInfo(light_pos, throughput, pdf, light_id,
                                     light_normal, light_normal,
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
    if (camera->intersect_lens(now_ray, record.distance, &px_pos, &lens_pos,
                               &objplane_pos, px_u, px_v)) {
      double g_term = dot(wi, camera->sensor_dir) *
                      dot(now_ray.dir, prev_orienting_normal) /
                      (lens_pos - now_ray.org).len2();
      throughput = throughput * g_term *
                   camera->get_bpt_coefficient(px_pos, lens_pos, objplane_pos);
      pdf *= pdf_sample_omega * dot(wi, camera->sensor_dir) /
             (lens_pos - now_ray.org).len2();

      vertices.emplace_back(HitpointInfo(lens_pos, throughput, pdf, -1,
                                         camera->sensor_dir, camera->sensor_dir,
                                         ObjectType::LENS));
      *final_value = throughput / pdf;
      break;
    }

    if (!scene_hit) {
      *final_value = INVALID_VEC;
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
      *final_value = INVALID_VEC;
      break;
    }
    pdf *= roullete_prob;

    vertices.emplace_back(HitpointInfo(
        record.hitpoint, throughput, pdf, record.id, orienting_normal,
        orienting_geom_normal, ObjectType::PRIMITIVE));

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() &&
        dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
      *final_value = INVALID_VEC;
      break;
    }

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wo = prim->bxdf->sample(wi, orienting_normal, into, rnd, mode);
    if (wo == INVALID_VEC) {
      *final_value = INVALID_VEC;
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
