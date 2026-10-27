#include "integrator.h"
#include "renderer/material.h"
#include "renderer/medium.h"
#include "renderer/ray.h"
#include "scene/primitive.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

void Integrator::pt_integrate(Scene *scene, const Ray &ray, Random *rnd) {
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
    if (prim->is_light()) {
      if (dot(record.normal, now_ray.dir) < 0.) {
        value = value + prim->emission * (throughput / pdf);
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
                  ? Ray(record.hitpoint + EPS * orienting_geom_normal, wi)
                  : Ray(record.hitpoint - EPS * orienting_geom_normal, wi);
    throughput = throughput *
                 prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                  orienting_geom_normal, into, mode) *
                 fabs(dot(wi, orienting_normal));

    if (throughput.is_lezero()) {
      break;
    }

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

void Integrator::mis_integrate(Scene *scene, const Ray &ray, Random *rnd) {
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
    if (prim->is_light() && dot(now_ray.dir, record.normal) < 0.) {
      if (depth == 0) {
        value = prim->emission;
        break;
      }

      Vec3 to_light = record.hitpoint - now_ray.org;
      double pdf_sample_light = scene->pdf_sample_light_uniform() *
                                prim->shape->pdf_sample_surface_area() *
                                to_light.len2() /
                                dot(now_ray.dir, -1. * record.normal);
      double mis_weight =
          prev_is_specular ? 1.
                           : prev_pdf_bxdf / (pdf_sample_light + prev_pdf_bxdf);

      value = value + prim->emission * mis_weight * (throughput / pdf);
      break;
    } else if (prim->is_light()) {
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
      test_ray = dot(to_light_norm, orienting_normal) > 0.
                     ? Ray(record.hitpoint + EPS * orienting_geom_normal,
                           to_light_norm)
                     : Ray(record.hitpoint - EPS * orienting_geom_normal,
                           to_light_norm);

      if (pdf_bxdf > 0. && scene->intersect(test_ray, &test_record) &&
          test_record.id == light_id &&
          dot(to_light_norm, test_record.normal) <= 0.) {
        pdf_sample_light = scene->pdf_sample_light_uniform() *
                           pdf_sample_surface_area * to_light.len2() /
                           dot(to_light_norm, -1. * test_record.normal);

        double mis_weight = pdf_sample_light / (pdf_sample_light + pdf_bxdf);

        Color tmp_throughput =
            throughput *
            prim->bxdf->eval(wo, to_light_norm, record.uv, orienting_normal,
                             orienting_geom_normal, into, mode) *
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

    if (throughput.is_lezero()) {
      break;
    }

    prev_pdf_bxdf =
        prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
    pdf *= prev_pdf_bxdf;
    prev_is_specular = prim->bxdf->is_specular();
  }
}

void Integrator::volpt_mis_integrate(Scene *scene, const Ray &ray,
                                     Random *rnd) {
  const IntegrateMode mode = IntegrateMode::PT;
  Ray now_ray = ray, test_ray = Ray(Point3(), Vec3());
  HitRecord record = HitRecord(), test_record = HitRecord();
  Color throughput = Color(1.);
  double pdf = 1., prev_pdf_material;
  bool prev_is_specular = false;
  Medium *now_medium = nullptr;

  for (uint32_t depth = 0;; depth++) {
    double sampled_distance =
        now_medium ? now_medium->sample_distance(rnd) : INF;
    record.reset(sampled_distance);
    bool scene_hit = scene->intersect(now_ray, &record);

    if (!scene_hit && !now_medium) {
      Vec2 uv = scene->get_skysphere_uv(now_ray.dir);
      value = value + scene->background->get_color(uv) * (throughput / pdf);
      break;
    }

    if (!scene_hit && now_medium) {
      // scatter in medium
      pdf *= now_medium->pdf_sample_distance(sampled_distance);
      throughput =
          throughput * now_medium->calc_transmittance(sampled_distance);
    } else if (scene_hit && now_medium) {
      // hit to end in medium
      pdf *= now_medium->pdf_hit_to_end(record.distance);
      throughput = throughput * now_medium->calc_transmittance(record.distance);
    }

    double roullete_prob = roullete(depth);
    if (rnd->gen() > roullete_prob) {
      break;
    }
    pdf *= roullete_prob;

    Vec3 wo = -1. * now_ray.dir;

    if (scene_hit) {
      Primitive *prim = &scene->primitives[record.id];
      if (prim->is_light() && dot(now_ray.dir, record.normal) < 0.) {
        if (depth == 0) {
          value = prim->emission;
          break;
        }

        Vec3 to_light = record.hitpoint - now_ray.org;
        double pdf_sample_light = scene->pdf_sample_light_uniform() *
                                  prim->shape->pdf_sample_surface_area() *
                                  to_light.len2() /
                                  dot(now_ray.dir, -1. * record.normal);

        prev_pdf_material *=
            (now_medium ? now_medium->pdf_hit_to_end(record.distance) : 1.);

        double mis_weight =
            prev_is_specular
                ? 1.
                : prev_pdf_material / (pdf_sample_light + prev_pdf_material);

        value = value + prim->emission * mis_weight * (throughput / pdf);
        break;
      } else if (prim->is_light()) {
        break;
      }

      Vec3 orienting_normal = dot(record.normal, now_ray.dir) < 0.
                                  ? record.normal
                                  : -1. * record.normal;
      Vec3 orienting_geom_normal =
          dot(record.geometry_normal, orienting_normal) > 0.
              ? record.geometry_normal
              : -1. * record.geometry_normal;
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
        test_ray = dot(to_light_norm, orienting_normal) > 0.
                       ? Ray(record.hitpoint + EPS * orienting_geom_normal,
                             to_light_norm)
                       : Ray(record.hitpoint - EPS * orienting_geom_normal,
                             to_light_norm);

        if (pdf_bxdf > 0. && scene->intersect(test_ray, &test_record) &&
            test_record.id == light_id &&
            dot(to_light_norm, test_record.normal) <= 0.) {
          pdf_sample_light = scene->pdf_sample_light_uniform() *
                             pdf_sample_surface_area * to_light.len2() /
                             dot(to_light_norm, -1. * test_record.normal);

          double pdf_hit_to_end =
              now_medium ? now_medium->pdf_hit_to_end(to_light.len()) : 1.;

          double mis_weight =
              pdf_sample_light / (pdf_sample_light + pdf_bxdf * pdf_hit_to_end);

          Color tmp_throughput =
              throughput *
              prim->bxdf->eval(wo, to_light_norm, record.uv, orienting_normal,
                               orienting_geom_normal, into, mode) *
              fabs(dot(to_light_norm, orienting_normal));
          value = value + light->emission * (tmp_throughput * mis_weight /
                                             (pdf * pdf_sample_light));
        }
      }

      Vec3 wi = prim->bxdf->sample(wo, orienting_normal, into, rnd, mode);
      if (wi == INVALID_VEC) {
        break;
      }

      bool is_transmit = dot(wi, orienting_geom_normal) < 0.;
      now_ray = is_transmit ? Ray(record.hitpoint - EPS * orienting_normal, wi)
                            : Ray(record.hitpoint + EPS * orienting_normal, wi);
      throughput = throughput *
                   prim->bxdf->eval(wo, wi, record.uv, orienting_normal,
                                    orienting_geom_normal, into, mode) *
                   fabs(dot(wi, orienting_normal));

      if (throughput.is_lezero()) {
        break;
      }

      prev_pdf_material =
          prim->bxdf->sampling_pdf(wo, wi, orienting_normal, into, mode);
      pdf *= prev_pdf_material;
      prev_is_specular = prim->bxdf->is_specular();

      if (is_transmit && into && prim->medium) {
        now_medium = prim->medium;
      } else if (is_transmit && !into && prim->medium) {
        now_medium = nullptr;
      }
    } else if (now_medium) {
      // scatter in medium
      Point3 scattering_point = now_ray.org + sampled_distance * now_ray.dir;

      int32_t light_id = scene->sample_light_uniform(rnd);
      if (light_id >= 0) {
        Primitive *light = &scene->primitives[light_id];
        double pdf_sample_light, pdf_sample_surface_area, pdf_medium;
        Point3 pos_on_light =
            light->shape->sample_surface(rnd, &pdf_sample_surface_area);
        Vec3 to_light = pos_on_light - scattering_point;
        Vec3 to_light_norm = normalize(to_light);

        test_record.reset(to_light.len() + 0.1);
        test_ray = Ray(scattering_point, to_light_norm);

        if (scene->intersect(test_ray, &test_record) &&
            test_record.id == light_id &&
            dot(to_light_norm, test_record.normal) <= 0.) {
          pdf_sample_light = scene->pdf_sample_light_uniform() *
                             pdf_sample_surface_area * to_light.len2() /
                             dot(to_light_norm, -1. * test_record.normal);

          pdf_medium = now_medium->eval_phasefn(wo, to_light_norm) *
                       now_medium->pdf_hit_to_end(test_record.distance);

          double mis_weight =
              pdf_sample_light / (pdf_sample_light + pdf_medium);

          Color tmp_throughput =
              throughput * now_medium->sigma_s *
              now_medium->eval_phasefn(wo, to_light_norm) *
              now_medium->calc_transmittance(test_record.distance);

          value = value + light->emission * (tmp_throughput * mis_weight /
                                             (pdf * pdf_sample_light));
        }
      }

      Vec3 wi = now_medium->sample_phasefn(wo, rnd);
      now_ray = Ray(scattering_point, wi);
      throughput = throughput * now_medium->sigma_s;

      if (throughput.is_lezero()) {
        break;
      }

      prev_pdf_material = now_medium->eval_phasefn(wo, wi);
      prev_is_specular = false;
    }
  }
}
