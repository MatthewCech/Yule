#pragma once
#include "console-utils.hpp"
#include "ParticleSystem.hpp"
#include "console-input.h"

// Please god i must be able to make this shorter
typedef std::chrono::steady_clock::time_point Timepoint;

/// <summary>
/// Particle specific data that isn't needed for the system to operate.
/// </summary>
struct ParticleData
{
public:
  char visual;
  double startLife;

  ParticleData() :
    visual('?')
    , startLife(200)
  { }
};

// Defines be here
#define CONSOLE_WIDTH (rlutil::tcols() - 1)
#define CONSOLE_HEIGHT (rlutil::trows())

// Function signature declarations
void ProcessInputChar(char key);
void ProcessInputString(std::string path);
void ResizeIfNeeded();

void DrawParticles(ParticleSystem<ParticleData>& particle_system);
RConsole::Color DetermineColor(Particle<ParticleData> p);
void CreateParticle(Particle<ParticleData>& p);
void CreateFileParticle(Particle<ParticleData>& p);
void UpdateParticle(double dt, Particle<ParticleData>& p);

void DrawFrameTime(bool is_displaying);
void DrawColorDisplay(bool is_displaying);
void DrawForegroundLog();
void DrawBackgroundLog();

