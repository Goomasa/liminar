#include "integrator.h"
#include "renderer/material.h"
#include "renderer/ray.h"
#include "util/utils.h"
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
                    dot(-1. * wo, prev_orienting_normal) /
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
      break;
    }
    pdf *= roullete_prob;

    vertices.emplace_back(HitpointInfo(
        record.hitpoint, throughput, pdf, record.id, orienting_normal,
        orienting_geom_normal, ObjectType::PRIMITIVE));

    bool into = dot(orienting_normal, record.normal) > 0.;
    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
    if (wi == INVALID_VEC) {
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
