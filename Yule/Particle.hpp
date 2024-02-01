#pragma once

#define PARTICLE_LIFE_DEFAULT 100


template <typename T> class Particle
{
public:
  Particle(const T &d, double x = 0, double y = 0, double vx = 0, double vy = 0, double life = PARTICLE_LIFE_DEFAULT)
    : Data(d)
    , PosX(x)
    , PosY(y)
    , VelX(vx)
    , VelY(vy)
    , Life(life)
    , Active(true)
  {  }

  T Data;
  double PosX;
  double PosY;
  double VelX;
  double VelY;
  double Life;
  bool Active;
};


