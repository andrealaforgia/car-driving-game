#include "clip.h"

static clip_vertex_t intersect(const clip_vertex_t* a, const clip_vertex_t* b,
                               double near_z) {
  double t = (near_z - a->position.z) / (b->position.z - a->position.z);
  clip_vertex_t vertex;
  vertex.position = vec3_lerp(a->position, b->position, t);
  vertex.position.z = near_z;
  vertex.u = a->u + (b->u - a->u) * t;
  vertex.v = a->v + (b->v - a->v) * t;
  return vertex;
}

int clip_to_near_plane(const clip_vertex_t* in, int count, double near_z,
                       clip_vertex_t* out) {
  int written = 0;
  for (int i = 0; i < count; ++i) {
    const clip_vertex_t* current = &in[i];
    const clip_vertex_t* next = &in[(i + 1) % count];
    int current_in = current->position.z >= near_z;
    int next_in = next->position.z >= near_z;
    if (current_in) {
      out[written++] = *current;
    }
    if (current_in != next_in) {
      out[written++] = intersect(current, next, near_z);
    }
  }
  return written;
}
