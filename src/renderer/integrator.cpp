#include "integrator.h"
#include "scene/primitive.h"
#include "util/utils.h"

inline double roullete(const uint32_t depth, const Color &throughput) {
  if (depth < DEPTH) {
    return 1.0;
  } else if (depth > MAX_DEPTH) {
    return throughput.max() * pow(0.5, depth - MAX_DEPTH);
  } else {
    return throughput.max();
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
    if (!scene->test_intersect(now_ray, &record)) {
      return;
    }

    double roullete_prob = roullete(depth, throughput);
    if (rnd->gen() > roullete_prob) {
      return;
    }
    pdf *= roullete_prob;

    Primitive *prim = &scene->primitives[record.id];
    orienting_normal = dot(record.normal, now_ray.dir) < 0.
                           ? record.normal
                           : -1. * record.normal;

    if (prim->is_light() && dot(record.normal, now_ray.dir) < 0.) {
      value = value + throughput * prim->emission / pdf;
      return;
    }

    Vec3 next_dir = prim->bxdf->sample(now_ray.dir, orienting_normal, rnd);
    Point3 next_org = record.hitpoint + EPS * orienting_normal;
    now_ray = Ray(next_org, next_dir);
    throughput =
        throughput * prim->bxdf->eval() * dot(next_dir, orienting_normal);
    pdf *= prim->bxdf->sampling_pdf(next_dir, orienting_normal);
  }
}
