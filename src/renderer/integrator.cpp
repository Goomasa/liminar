#include "integrator.h"
#include "scene/primitive.h"
#include "util/utils.h"

inline double roullete(const uint32_t depth, const Color &throughput) {
  if (depth < DEPTH) {
    return 1.0;
  } else if (depth > MAX_DEPTH) {
    return normalize(throughput).max() * pow(0.5, depth - MAX_DEPTH);
  } else {
    return normalize(throughput).max();
  }
}

void Integrator::integrate(Scene *scene, Ray ray, Random *rnd) {
  Ray now_ray = ray;
  HitRecord record = HitRecord();
  Color throughput = Color(1.);
  double pdf = 1.;
  Vec3 orienting_normal;

  for (uint32_t depth = 0;; depth++) {
    record.reset();
    if (!scene->intersect(now_ray, &record)) {
      value = scene->background * (throughput / pdf);
      return;
    }

    Primitive *prim = &scene->primitives[record.id];
    if (prim->is_light() && dot(record.normal, now_ray.dir) < 0.) {
      value = value + prim->emission * (throughput / pdf);
      return;
    }

    double roullete_prob = roullete(depth, throughput);
    if (rnd->gen() > roullete_prob) {
      return;
    }
    pdf *= roullete_prob;

    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;

    Vec3 next_dir = prim->bxdf->sample(now_ray.dir, orienting_normal, rnd);
    now_ray = Ray(record.hitpoint, next_dir, record.id);
    throughput = throughput * prim->bxdf->eval(next_dir, orienting_normal) *
                 fmax(dot(next_dir, orienting_normal), 0.);
    pdf *= prim->bxdf->sampling_pdf(next_dir, orienting_normal);
  }
}
