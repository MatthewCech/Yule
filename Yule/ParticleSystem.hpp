#pragma once
#include <list>
#include <functional>
#include <iostream>
#include "Particle.hpp"

template <typename T> class ParticleSystem
{
public:
  /// <summary>
  /// Constructor. Requires explicit parameters, no such thing as a default constructor here.
  /// </summary>
  /// <param name="max"></param>
  /// <param name="spawn_delay_ms"></param>
  /// <param name="loops"></param>
  /// <param name="def"></param>
  /// <param name="create"></param>
  ParticleSystem(int max, double spawn_delay_seconds, bool loops, const T& def, std::function<void(Particle<T>&)> configure, std::function<void(double, Particle<T>&)> pre_update)
    : posX_(0)
    , posY_(0)
    , maxParticles_(max)
    , maxLifeSeconds_(5)
    , spawnDelaySeconds_(spawn_delay_seconds)
    , spawnCounter_(spawnDelaySeconds_)
    , oneshotCount_(0)
    , isLooping_(loops)
    , default_(def)
    , particles_()
    , configureNewParticle_(configure)
    , preUpdate_(pre_update)
  {  }

  /// <summary>
  /// Primary update, should be called every loop. DT is measured in seconds
  /// </summary>
  /// <param name="dt">seconds since last update</param>
  void Update(double dt)
  {
    // Handle pre-update if present
    if (preUpdate_ != nullptr)
    {
      for (Particle<T>& p : particles_)
      {
        preUpdate_(dt, p);
      }
    }

    // If necessary, add new particles.
    // If there is no delay, add them all immediately.
    const bool oneshotIncomplete = !isLooping_ && oneshotCount_ < maxParticles_;
    if (isLooping_ || oneshotIncomplete)
    {
      if (spawnDelaySeconds_ == 0)
      {
        while (particles_.size() < maxParticles_)
        {
          AddParticle();
        }
      }
      else if (spawnCounter_ >= spawnDelaySeconds_)
      {
        AddParticle();
        spawnCounter_ = 0;
      }
    }

    spawnCounter_ += dt;

    // Update all
    for (Particle<T>& p : particles_)
    {
      p.Life -= dt;

      if (p.Life <= 0)
      {
        p.Active = false;
      }

      p.PosX += p.VelX * dt;
      p.PosY += p.VelY * dt;
    }

    // Remove every particle no longer marked as active.
    particles_.remove_if([](const Particle<T> &p) 
    { 
      return p.Active == false; 
    });
  }

  /// <summary>
  /// Adds a particle, differing to provided function for setting specifics
  /// like velocity, data, etc.
  /// </summary>
  void AddParticle()
  {
    Particle<T> p1 = Particle<T>(default_, posX_, posY_, 0, 1, maxLifeSeconds_);

    if (configureNewParticle_ != nullptr)
    {
      configureNewParticle_(p1);
    }

    if (!isLooping_)
    {
      ++oneshotCount_;
    }

    particles_.push_back(p1);
  }

  // Allows access to things
  std::list<Particle<T>> &Particles() { return particles_; }
  size_t GetMaxParticles()            { return maxParticles_; }
  double GetPosX()                    { return posX_; }
  double GetPosY()                    { return posY_; }
  double GetSpawnDelay()              { return spawnDelaySeconds_; }
  void SetPos(double x, double y)     { posX_ = x; posY_ = y; }
  void SetPosX(double x)              { posX_ = x; }
  void SetPosY(double y)              { posY_ = y; }
  void SetMaxParticles(size_t max)    { maxParticles_ = max; }
  void SetSpawnDelay(double delay)    { spawnDelaySeconds_ = delay; }

protected:
  // Variables
  size_t maxParticles_;
  double posX_;
  double posY_;
  double maxLifeSeconds_;
  double spawnDelaySeconds_;
  double spawnCounter_;
  int oneshotCount_;
  bool isLooping_;
  const T default_;
  std::list<Particle<T>> particles_;
  std::function<void(Particle<T>&)> configureNewParticle_;
  std::function<void(double, Particle<T>&)> preUpdate_;
};
