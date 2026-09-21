#pragma once

#include <QtGlobal>

#include <cmath>

namespace navi::dockanim {

constexpr qreal kPi = 3.14159265358979323846;

/// Smooth cosine falloff: 1 at distance 0, 0 at/after radius.
inline qreal falloff(qreal distance, qreal radius) {
  if (distance >= radius)
    return 0.0;
  if (distance <= 0.0)
    return 1.0;
  const qreal x = distance / radius;
  return 0.5 * (1.0 + std::cos(x * kPi));
}

/// EaseOutBack (c1 = 1.70158): rises past 1 and settles — the "pop".
inline qreal easeOutBack(qreal t) {
  if (t <= 0.0)
    return 0.0;
  if (t >= 1.0)
    return 1.0;
  constexpr qreal c1 = 1.70158;
  constexpr qreal c3 = c1 + 1.0;
  const qreal u = t - 1.0;
  return 1.0 + c3 * u * u * u + c1 * u * u;
}

/// Click feedback: 1 → (1 - depth) → 1 as t sweeps [0,1].
inline qreal pressDip(qreal t, qreal depth) {
  if (t <= 0.0 || t >= 1.0)
    return 1.0;
  return 1.0 - depth * std::sin(t * kPi);
}

} // namespace navi::dockanim
