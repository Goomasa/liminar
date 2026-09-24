#include "integrator.h"
#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/primitive.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

constexpr uint32_t DEPTH = 5;
constexpr uint32_t MAX_DEPTH = 10;

inline double roullete(const uint32_t depth) {
  if (depth < DEPTH) {
    return 1.0;
  } else if (depth > MAX_DEPTH) {
    return 0.;
  } else {
    return 0.8;
  }
}

void Integrator::pt_integrate(Scene *scene, Ray ray, Random *rnd) {
  const IntegrateMode mode = IntegrateMode::PT;
  Ray now_ray = ray;
  HitRecord record = HitRecord();
  Color throughput = Color(1.);
  double pdf = 1.;
  Vec3 orienting_normal, orienting_geom_normal, wo;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      Vec2 uv = scene->get_skysphere_uv(now_ray.dir);
      value = value + scene->background->get_color(uv) * (throughput / pdf);
      break;
    }

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() && dot(record.normal, now_ray.dir) < 0.) {
      value = value + prim->emission * (throughput / pdf);
      break;
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      break;
    }
    pdf *= roullete_prob;

    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;
    orienting_geom_normal = dot(record.geometry_normal, orienting_normal) > 0.
                                ? record.geometry_normal
                                : -1. * record.geometry_normal;

    wo = -1. * now_ray.dir;
    bool into = dot(orienting_normal, record.normal) > 0.;

    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
    if (wi == INVALID_VEC) {
      break;
    }

    now_ray = dot(wi, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_normal, wi)
                  : Ray(record.hitpoint - EPS * orienting_normal, wi);
    throughput = throughput *
                 prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                  orienting_geom_normal, into, mode) *
                 fabs(dot(wi, orienting_normal));
    pdf *= prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
  }
}

/*
void Integrator::mis_integrate(Scene *scene, Ray ray, Random *rnd) {
  Ray now_ray = ray, test_ray = Ray(Point3(), Vec3());
  HitRecord record = HitRecord(), test_record = HitRecord();
  Color throughput = Color(1.);
  double pdf = 1.;
  Vec3 orienting_normal, wo;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      Vec2 uv = scene->get_skysphere_uv(now_ray.dir);
      value = value + scene->background->get_color(uv) * (throughput / pdf);
      break;
    }

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() &&
        dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
      if (depth == 0) {
        value = prim->emission;
      }

      break;
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      break;
    }
    pdf *= roullete_prob;

    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;
    wo = -1. * now_ray.dir;
    bool into = dot(orienting_normal, record.normal) > 0.;

    int32_t light_id = scene->sample_light_uniform(rnd);
    if (!prim->bxdf->is_specular() && light_id >= 0) {
      Primitive *light = &scene->primitives[light_id];
      double pdf_sample_light, pdf_sample_surface_area, pdf_bxdf;
      Point3 pos_on_light =
          light->shape->sample_surface(rnd, &pdf_sample_surface_area);
      Vec3 to_light = pos_on_light - record.hitpoint;
      Vec3 to_light_norm = normalize(to_light);

      pdf_bxdf =
          prim->bxdf->sampling_pdf(wo, to_light_norm, orienting_normal, into);

      test_record.reset(to_light.len() + 0.1);
      test_ray = Ray(record.hitpoint, to_light_norm, record.id);

      if (pdf_bxdf > 0. && scene->intersect(test_ray, &test_record) &&
          test_record.id == light_id &&
          dot(to_light_norm, light->shape->get_normal(pos_on_light)) <= 0.) {
        pdf_sample_light =
            scene->pdf_sample_light_uniform() * pdf_sample_surface_area *
            to_light.len2() /
            dot(to_light_norm, -1. * light->shape->get_normal(pos_on_light));

        double mis_weight = pdf_sample_light / (pdf_sample_light + pdf_bxdf);

        Color tmp_throughput =
            throughput *
            prim->bxdf->eval(wo, to_light_norm, orienting_normal, into) *
            fabs(dot(to_light_norm, orienting_normal));
        value = value + light->emission * (tmp_throughput * mis_weight /
                                           (pdf * pdf_sample_light));
      }
    }

    {
      test_record.reset();
      Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd);
      test_ray = Ray(record.hitpoint, wi, record.id);

      if (scene->intersect(test_ray, &test_record)) {
        Primitive *light = &scene->primitives[test_record.id];
        if (light->is_light() &&
            dot(wi, light->shape->get_normal(test_record.hitpoint)) <= 0.) {
          double pdf_bxdf =
              prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into);

          Vec3 to_light = test_record.hitpoint - record.hitpoint;
          double pdf_sample_light =
              scene->pdf_sample_light_uniform() *
              light->shape->pdf_sample_surface_area() * to_light.len2() /
              dot(wi, -1. * light->shape->get_normal(test_record.hitpoint));
          double mis_weight = prim->bxdf->is_specular()
                                  ? 1.
                                  : pdf_bxdf / (pdf_sample_light + pdf_bxdf);

          Color tmp_throughput =
              throughput * prim->bxdf->eval(wo, wi, orienting_normal, into) *
              fabs(dot(wi, orienting_normal));

          value = value + light->emission *
                              (tmp_throughput * mis_weight / (pdf * pdf_bxdf));
          break;
        }
      }
    }

    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd);
    now_ray = Ray(record.hitpoint, wi, record.id);
    throughput = throughput * prim->bxdf->eval(wo, wi, orienting_normal, into) *
                 fabs(dot(wi, orienting_normal));
    pdf *= prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into);
  }
}
*/

void Integrator::mis_integrate(Scene *scene, Ray ray, Random *rnd) {
  const IntegrateMode mode = IntegrateMode::PT;
  Ray now_ray = ray, test_ray = Ray(Point3(), Vec3());
  HitRecord record = HitRecord(), test_record = HitRecord();
  Color throughput = Color(1.);
  double pdf = 1., prev_pdf_bxdf;
  bool prev_is_specular = false;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      Vec2 uv = scene->get_skysphere_uv(now_ray.dir);
      value = value + scene->background->get_color(uv) * (throughput / pdf);
      break;
    }

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() &&
        dot(now_ray.dir, prim->shape->get_normal(record.hitpoint)) < 0.) {
      if (depth == 0) {
        value = prim->emission;
        break;
      }

      Vec3 to_light = record.hitpoint - now_ray.org;
      double pdf_sample_light =
          scene->pdf_sample_light_uniform() *
          prim->shape->pdf_sample_surface_area() * to_light.len2() /
          dot(normalize(to_light),
              -1. * prim->shape->get_normal(record.hitpoint));
      double mis_weight =
          prev_is_specular ? 1.
                           : prev_pdf_bxdf / (pdf_sample_light + prev_pdf_bxdf);

      value = value + prim->emission * mis_weight * (throughput / pdf);
      break;
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      break;
    }
    pdf *= roullete_prob;

    Vec3 orienting_normal = dot(record.normal, now_ray.dir) < 0.
                                ? record.normal
                                : -1. * record.normal;
    Vec3 orienting_geom_normal =
        dot(record.geometry_normal, orienting_normal) > 0.
            ? record.geometry_normal
            : -1. * record.geometry_normal;
    Vec3 wo = -1. * now_ray.dir;
    bool into = dot(orienting_normal, record.normal) > 0.;

    // light sampling
    int32_t light_id = scene->sample_light_uniform(rnd);
    if (!prim->bxdf->is_specular() && light_id >= 0) {
      Primitive *light = &scene->primitives[light_id];
      double pdf_sample_light, pdf_sample_surface_area, pdf_bxdf;
      Point3 pos_on_light =
          light->shape->sample_surface(rnd, &pdf_sample_surface_area);
      Vec3 to_light = pos_on_light - record.hitpoint;
      Vec3 to_light_norm = normalize(to_light);

      pdf_bxdf = prim->bxdf->sampling_pdf(wo, to_light_norm, orienting_normal,
                                          into, mode);

      test_record.reset(to_light.len() + 0.1);
      test_ray =
          dot(to_light_norm, orienting_normal) > 0.
              ? Ray(record.hitpoint + EPS * orienting_normal, to_light_norm)
              : Ray(record.hitpoint - EPS * orienting_normal, to_light_norm);

      if (pdf_bxdf > 0. && scene->intersect(test_ray, &test_record) &&
          test_record.id == light_id &&
          dot(to_light_norm, light->shape->get_normal(pos_on_light)) <= 0.) {
        pdf_sample_light =
            scene->pdf_sample_light_uniform() * pdf_sample_surface_area *
            to_light.len2() /
            dot(to_light_norm, -1. * light->shape->get_normal(pos_on_light));

        double mis_weight = pdf_sample_light / (pdf_sample_light + pdf_bxdf);

        Color tmp_throughput =
            throughput *
            prim->bxdf->eval(wo, to_light_norm, test_record.uv,
                             orienting_normal, orienting_geom_normal, into,
                             mode) *
            fabs(dot(to_light_norm, orienting_normal));
        value = value + light->emission * (tmp_throughput * mis_weight /
                                           (pdf * pdf_sample_light));
      }
    }

    Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
    if (wi == INVALID_VEC) {
      break;
    }

    now_ray = dot(wi, orienting_normal) > 0.
                  ? Ray(record.hitpoint + EPS * orienting_normal, wi)
                  : Ray(record.hitpoint - EPS * orienting_normal, wi);
    throughput = throughput *
                 prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                  orienting_geom_normal, into, mode) *
                 fabs(dot(wi, orienting_normal));
    prev_pdf_bxdf =
        prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
    pdf *= prev_pdf_bxdf;
    prev_is_specular = prim->bxdf->is_specular();
  }
}
