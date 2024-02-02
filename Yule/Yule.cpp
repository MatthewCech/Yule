#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <stack>
#include "console-utils.hpp"
#include "ParticleSystem.hpp"
#include "Yule.hpp"
#include "console-input.h"

// Global Variables (oops, but not sorry)
int windowWidth;
int windowHeight;
long lastFrameMicroseconds = 1000; // Use a default value of 1 millisecond to have at least something there.
bool displayFrameTime = false;
bool displayColors = false;

bool pendingScrapeData = false;
char scraped[200];
std::stack<char> pendingBurnStack;

/// <summary>
/// IT'S MAIN BAYBEEEEE
/// </summary>
/// <returns>never</returns>
int main()
{
  // Data config/setup
  ParticleData data = ParticleData();
  ParticleSystem<ParticleData> particleSystem = ParticleSystem<ParticleData>(100, 0.015, true, data, CreateParticle, UpdateParticle);
  ParticleSystem<ParticleData>* scrapeSys = nullptr;
  InputParser parser = InputParser();
  
  // Console config/setup
  windowWidth = CONSOLE_WIDTH;
  windowHeight = CONSOLE_HEIGHT;
  RConsole::Canvas::ReInit(windowWidth, windowHeight);
  RConsole::Canvas::SetCursorVisible(false);
  RConsole::Canvas::FillCanvas();

  while (true)
  {
    // Prepare
    Timepoint start = std::chrono::steady_clock::now();
    const double lastFrameMS = lastFrameMicroseconds / 1000.0;
    const double lastFrameS = lastFrameMS / 1000.0;
    ResizeIfNeeded();

    // Update
    parser.HandleInput(ProcessInputChar, ProcessInputString);
    particleSystem.Update(lastFrameS);
    RConsole::Canvas::Update();
    HandlePendingScrapedData(scrapeSys, data, lastFrameS);
    TryUpdate(scrapeSys, lastFrameS);

    // Draw
    DrawBackgroundLog();
    DrawParticles(particleSystem);
    DrawParticles(scrapeSys);
    DrawForegroundLog();
    DrawFrameTime(displayFrameTime);
    DrawColorDisplay(displayColors);

    // Resolve
    std::this_thread::yield(); // Be polite! 
    std::this_thread::sleep_until(start + std::chrono::milliseconds(4)); // Max speed of 
    Timepoint end = std::chrono::steady_clock::now();
    lastFrameMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  }

  delete scrapeSys;
  return 0;
}

/// <summary>
/// Wrap to cover null system updates
/// </summary>
/// <param name="particle_system"></param>
/// <param name="dt"></param>
void TryUpdate(ParticleSystem<ParticleData>* particle_system, const double& dt)
{
  if (particle_system != nullptr)
  {
    TryUpdate(*particle_system, dt);
  }
}

/// <summary>
/// Wraps update call to more easily wrangle later if needed
/// </summary>
/// <param name="particle_system"></param>
/// <param name="dt"></param>
void TryUpdate(ParticleSystem<ParticleData>& particle_system, const double& dt)
{
  particle_system.Update(dt);
}

/// <summary>
/// Gross but extracted method to wrangle pushing characters into the pending particle stack
/// </summary>
/// <param name="scrapeSys"></param>
/// <param name="data"></param>
/// <param name="lastFrameS"></param>
void HandlePendingScrapedData(ParticleSystem<ParticleData>*& scrapeSys, ParticleData& data, const double& dt)
{
  if (!pendingScrapeData)
  {
    return;
  }

  if (scrapeSys != nullptr)
  {
    RConsole::Canvas::FillCanvas();
    delete scrapeSys;
  }

  scrapeSys = new ParticleSystem<ParticleData>(sizeof(scraped), 0.003, false, data, CreateFileParticle, UpdateParticle);
  for (int i = 0; i < sizeof(scraped); ++i)
  {
    char entry = scraped[i];

    // Prevent bell
    if (entry == static_cast<unsigned char>(7))
    {
      entry = '?';
    }

    pendingBurnStack.push(entry);
  }

  pendingScrapeData = false;
}

/// <summary>
/// Given a system, goes through and draws the particle, extracting the visual from the data.
/// </summary>
/// <param name="particle_system"></param>
void DrawParticles(ParticleSystem<ParticleData>& particle_system)
{
  std::list<Particle<ParticleData>> particles = particle_system.Particles();
  for (Particle<ParticleData>& p : particles)
  {
    RConsole::Canvas::Draw(p.Data.visual, static_cast<float>(p.PosX), static_cast<float>(p.PosY), DetermineColor(p));
  }
}

/// <summary>
/// Given a system, goes through and draws the particle, extracting the visual from the data.
/// </summary>
/// <param name="particle_system"></param>
void DrawParticles(ParticleSystem<ParticleData>* particle_system)
{
  if (particle_system != nullptr)
  {
    DrawParticles(*particle_system);
  }
}

/// <summary>
/// Shows the number of milliseconds the last frame took 
/// </summary>
/// <param name="is_displaying"></param>
void DrawFrameTime(bool is_displaying)
{
  if (!is_displaying)
  {
    return;
  }

  std::string composedFPS = std::to_string(lastFrameMicroseconds / 1000) + "." + std::to_string(lastFrameMicroseconds % 1000) + "ms";
  RConsole::Canvas::DrawString(composedFPS.c_str(), 0, 0, RConsole::DARKGREY);
  RConsole::Canvas::DrawString("(toggle with d or f)", 0, 1, RConsole::DARKGREY);
}

/// <summary>
/// Draws a debug color display to explore terminal settings and see what each color corresponds to
/// </summary>
/// <param name="is_displaying"></param>
void DrawColorDisplay(bool is_displaying)
{
  if (!is_displaying)
  {
    return;
  }

  const unsigned char testAscii[]
  {
    static_cast<unsigned char>(219),
    static_cast<unsigned char>(178),
    static_cast<unsigned char>(177),
    static_cast<unsigned char>(176)
  };

  const std::string colorStrings[]
  {
    "Black",
    "Blue",
    "Green",
    "Cyan",
    "Red",
    "Magenta",
    "Brown",
    "Grey",
    "Darkgrey",
    "Lightblue",
    "Lightgreen",
    "Lightcyan",
    "Lightred",
    "Lightmagenta",
    "Yellow",
    "White"
  };

  const int horizontalOffset = 1;
  const int verticalOffset = 2;

  for (int row = 0; row < static_cast<int>(RConsole::Color::PREVIOUS_COLOR); ++row)
  {
    for (int col = 0; col < sizeof(testAscii); ++col)
    {
      RConsole::Canvas::Draw(testAscii[col], col + horizontalOffset, row + verticalOffset, static_cast<RConsole::Color>(row));
      RConsole::Canvas::DrawString(colorStrings[row].c_str(), sizeof(testAscii) + 1, row + verticalOffset, static_cast<RConsole::Color>(row));
    }
  }

  RConsole::Canvas::DrawString("(toggle with c)", horizontalOffset, verticalOffset + static_cast<int>(RConsole::Color::PREVIOUS_COLOR), RConsole::DARKGREY);
}

/// <summary>
/// Consumes extended input
/// </summary>
/// <param name="path"></param>
void ProcessInputString(std::string path)
{
  if (path.length() > 2 && path[0] == '"' && path[path.length() - 1] == '"')
  {
    path = path.substr(1, path.length() - 2);
  }
  
  // Create our objects.
  std::ifstream fileObject(path, std::ios::binary);
 
  // See if we opened the file successfully.
  if (!fileObject.is_open())
  {
    return;
  }

  fileObject.read(scraped, sizeof(scraped));
  pendingScrapeData = true;

  fileObject.close();
}

/// <summary>
/// Consumes single item input
/// </summary>
void ProcessInputChar(char key)
{
  switch (key)
  {
    case 'd':
    case 'f':
      displayFrameTime = !displayFrameTime;
      break;

    case 'c':
      displayColors = !displayColors;
      break;
  }
}

/// <summary>
/// Monitors window size and handles canvas re-initialization if needed.
/// </summary>
void ResizeIfNeeded()
{
  const int windowFrameWidth = CONSOLE_WIDTH;
  const int windowFrameHeight = CONSOLE_HEIGHT;

  if (windowWidth != windowFrameWidth || windowHeight != windowFrameHeight)
  {
    windowWidth = windowFrameWidth;
    windowHeight = windowFrameHeight;

    RConsole::Canvas::ReInit(windowFrameWidth, windowFrameHeight);
    RConsole::Canvas::ForceClearEverything();
    RConsole::Canvas::SetCursorVisible(false);
  }
}


/// <summary>
/// Passed as argument to allow creation of new particles to take custom parameters.
/// </summary>
/// <param name="p"></param>
void CreateParticle(Particle<ParticleData>& p)
{
  p.VelX = (rand() % 200 - 100) / 30.0;
  p.VelY = (rand() % 200 - 100) / 10.0;
  p.PosX = windowWidth / 2 + (rand() % 8 - 4);
  p.PosY = windowHeight - 3;

  const double rand0to30 = (rand() % 30000 / 1000.0);
  p.Life = 2 - log10(rand0to30 + .001);

  p.Data.startLife = p.Life;

  //p.Data.visual = static_cast<unsigned char>((rand() % (255 - 32)) + 32); // Random ascii that ISN'T 7! NO USING ASCII-7, THAT'S SYSTEM BEEP NOISE

  const unsigned char charset[]
  {
    static_cast<unsigned char>(4),   // diamond
    static_cast<unsigned char>(4),   // diamond
    static_cast<unsigned char>(4),   // diamond
    static_cast<unsigned char>(249), // dot
    static_cast<unsigned char>(254), // square (centered)
    static_cast<unsigned char>(254), // square (centered)
  };
  p.Data.visual = charset[rand() % 6];
}

/// <summary>
/// An effect designed to look like throwing a piece of cardboard or paper into a fire - a 'fwoosh' if you will.
/// </summary>
/// <param name="p"></param>
void CreateFileParticle(Particle<ParticleData>& p)
{
  p.VelX = (rand() % 1000 / 1000.0 - 0.5) * 20;
  p.VelY = (rand() % 1000 / 1000.0) * -5;
  p.PosX = windowWidth / 2 + (rand() % 8 - 4);
  p.PosY = windowHeight - 1;

  const double rand0to30 = (rand() % 30000 / 1000.0);
  p.Life = 2.5 - log10(rand0to30 + .001);

  p.Data.startLife = p.Life;
  p.Data.visual = pendingBurnStack.top();
  pendingBurnStack.pop();
}

/// <summary>
/// Passed as argument to allow custom particle updating.
/// </summary>
/// <param name="p"></param>
void UpdateParticle(double dt, Particle<ParticleData>& p)
{
  p.VelY -= 5 * dt;
}

/// <summary>
/// Correlate a particle color to a given 
/// </summary>
/// <param name="p"></param>
/// <returns></returns>
RConsole::Color DetermineColor(Particle<ParticleData> p)
{
  double t = p.Life / p.Data.startLife;

  //if (t > 0.9)
  //{
  //  return RConsole::Color::WHITE;
  //}

  if (t > 0.5)
  {
    return RConsole::Color::YELLOW;
  }

  if (t > 0.3)
  {
    return RConsole::Color::LIGHTRED;
  }

  if (t > 0.15)
  {
    return RConsole::Color::RED;
  }

  return RConsole::Color::DARKGREY;
}

/// <summary>
/// Draws a log intended for the background, pixel by pixel!
/// </summary>
void DrawBackgroundLog()
{
  const unsigned char intensity[]
  {
    static_cast<unsigned char>(219),
    static_cast<unsigned char>(178),
    static_cast<unsigned char>(177),
    static_cast<unsigned char>(176)
  };

  // Image is 14x5
  const int x = windowWidth / 2 - (14 / 2);
  const int y = windowHeight - 5;

  RConsole::Color brown = RConsole::Color::BROWN;
  RConsole::Color yellow = RConsole::Color::YELLOW;
  RConsole::Color grey = RConsole::Color::DARKGREY;

  RConsole::Canvas::Draw(intensity[0], x + 1, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 2, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 3, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 4, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 5, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 6, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 7, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 8, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 9, y + 0, brown);
  RConsole::Canvas::Draw(intensity[0], x + 10, y + 0, brown);
  RConsole::Canvas::Draw(intensity[2], x + 11, y + 0, brown);
  RConsole::Canvas::Draw(intensity[2], x + 12, y + 0, brown);

  RConsole::Canvas::Draw(intensity[0], x + 0, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 1, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 2, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 3, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 4, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 5, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 6, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 7, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 8, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 9, y + 1, brown);
  RConsole::Canvas::Draw(intensity[2], x + 10, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 11, y + 1, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 12, y + 1, yellow);
  RConsole::Canvas::Draw(intensity[2], x + 13, y + 1, brown);

  RConsole::Canvas::Draw(intensity[1], x + 0, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 1, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 2, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 3, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 4, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 5, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 6, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 7, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 8, y + 2, brown);
  RConsole::Canvas::Draw(intensity[3], x + 9, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 10, y + 2, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 11, y + 2, yellow);
  RConsole::Canvas::Draw(intensity[0], x + 12, y + 2, yellow);
  RConsole::Canvas::Draw(intensity[2], x + 13, y + 2, brown);

  RConsole::Canvas::Draw(intensity[1], x + 0, y + 3, brown);
  RConsole::Canvas::Draw(intensity[1], x + 1, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 2, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 3, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 4, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 5, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 6, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 7, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 8, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 9, y + 3, brown);
  RConsole::Canvas::Draw(intensity[1], x + 10, y + 3, yellow);
  RConsole::Canvas::Draw(intensity[2], x + 11, y + 3, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 12, y + 3, yellow);
  RConsole::Canvas::Draw(intensity[2], x + 13, y + 3, brown);

  RConsole::Canvas::Draw(intensity[3], x + 1, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 2, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 3, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 4, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 5, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 6, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 7, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 8, y + 4, grey);
  RConsole::Canvas::Draw(intensity[3], x + 9, y + 4, brown);
  RConsole::Canvas::Draw(intensity[3], x + 10, y + 4, brown);
  RConsole::Canvas::Draw(intensity[2], x + 11, y + 4, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 12, y + 4, brown);
}

/// <summary>
/// Draws a log intended for the foreground, pixel by pixel!
/// </summary>
void DrawForegroundLog()
{
  const unsigned char intensity[]
  {
    static_cast<unsigned char>(219),
    static_cast<unsigned char>(178),
    static_cast<unsigned char>(177),
    static_cast<unsigned char>(176)
  };

  // Image is 9x7
  const int x = windowWidth / 2 - (9 / 2);
  const int y = windowHeight - 7;

  RConsole::Color brown = RConsole::Color::BROWN;
  RConsole::Color yellow = RConsole::Color::YELLOW;
  RConsole::Color grey = RConsole::Color::DARKGREY;

  RConsole::Canvas::Draw(intensity[0], x + 6, y + 0, brown);
  RConsole::Canvas::Draw(intensity[2], x + 7, y + 0, brown);

  RConsole::Canvas::Draw(intensity[0], x + 5, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 6, y + 1, brown);
  RConsole::Canvas::Draw(intensity[1], x + 7, y + 1, brown);
  RConsole::Canvas::Draw(intensity[2], x + 8, y + 1, brown);

  RConsole::Canvas::Draw(intensity[0], x + 3, y + 2, brown);
  RConsole::Canvas::Draw(intensity[0], x + 4, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 5, y + 2, brown);
  RConsole::Canvas::Draw(intensity[1], x + 6, y + 2, brown);
  RConsole::Canvas::Draw(intensity[2], x + 7, y + 2, brown);
  RConsole::Canvas::Draw(intensity[3], x + 8, y + 2, brown);

  RConsole::Canvas::Draw(intensity[0], x + 2, y + 3, brown);
  RConsole::Canvas::Draw(intensity[1], x + 3, y + 3, brown);
  RConsole::Canvas::Draw(intensity[1], x + 4, y + 3, brown);
  RConsole::Canvas::Draw(intensity[1], x + 5, y + 3, brown);
  RConsole::Canvas::Draw(intensity[2], x + 6, y + 3, brown);
  RConsole::Canvas::Draw(intensity[3], x + 7, y + 3, brown);

  RConsole::Canvas::Draw(intensity[0], x + 1, y + 4, brown);
  RConsole::Canvas::Draw(intensity[1], x + 2, y + 4, brown);
  RConsole::Canvas::Draw(intensity[1], x + 3, y + 4, brown);
  RConsole::Canvas::Draw(intensity[2], x + 4, y + 4, brown);
  RConsole::Canvas::Draw(intensity[2], x + 5, y + 4, brown);
  RConsole::Canvas::Draw(intensity[3], x + 6, y + 4, brown);

  RConsole::Canvas::Draw(intensity[2], x + 0, y + 5, brown);
  RConsole::Canvas::Draw(intensity[0], x + 1, y + 5, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 2, y + 5, yellow);
  RConsole::Canvas::Draw(intensity[2], x + 3, y + 5, brown);
  RConsole::Canvas::Draw(intensity[3], x + 4, y + 5, brown);
  RConsole::Canvas::Draw(intensity[3], x + 5, y + 5, brown);

  RConsole::Canvas::Draw(intensity[2], x + 0, y + 6, brown);
  RConsole::Canvas::Draw(intensity[2], x + 1, y + 6, yellow);
  RConsole::Canvas::Draw(intensity[1], x + 2, y + 6, yellow);
  RConsole::Canvas::Draw(intensity[3], x + 3, y + 6, brown);
  RConsole::Canvas::Draw(intensity[3], x + 4, y + 6, grey);
}
