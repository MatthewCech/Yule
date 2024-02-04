// General drawing stuff
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>

// Recycle bin utilities
#include <Windows.h>  // I mean, recycle bin is a fairly windows thing, so... yeah this is for all the caps stuff.
#include <locale>     // std::wstring_convert.
#include <codecvt>    // std::codecvt_utf8_utf16. c++2017 convert. Planned to be deprecated in c++26, apparently.
#include <string>     // std::string, std::wstring.
#include <iostream>   // std::cout.

// Yule specific stuff
#include "console-utils.hpp"
#include "ParticleSystem.hpp"
#include "Yule.hpp"
#include "console-input.h"

// Global Variables (oops, but not sorry)
int windowWidth;
int windowHeight;
long lastFrameMicroseconds = 1000; // Use a default value of 1 millisecond to have at least something there.
int numberBurned = 0;

bool displayFrameTime = false; // Input tracking for frame time
bool displayColors = false;    // Input tracking for color debug display
bool displayBurnCount = true;  // Input tracking for default file burnt count display

bool pendingScrapeData = false; // Scrape tracking: Basically, the file to 'burn'
int scrapedLocation = 0;        // Scrape tracking: Stack of characters left to 'burn'
char scraped[200];              // Scrape tracking: First N bytes of the file specified by size

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wstringConverter; 

void Clear()
{
  RConsole::Canvas::ReInit(windowWidth, windowHeight);
  RConsole::Canvas::ForceClearEverything();
  RConsole::Canvas::SetCursorVisible(false);
}

/// <summary>
/// IT'S MAIN BAYBEEEEE
/// </summary>
/// <returns>never</returns>
int main()
{
  // Data config/setup
  ParticleData data = ParticleData();
  ParticleSystem<ParticleData> flameParticles = ParticleSystem<ParticleData>(100, 0.015, true, data, CreateParticle, UpdateParticle);
  ParticleSystem<ParticleData>* fileParticles = nullptr;
  InputParser parser = InputParser();
  
  // Console config/setup
  windowWidth = CONSOLE_WIDTH;
  windowHeight = CONSOLE_HEIGHT;
  RConsole::Canvas::ReInit(windowWidth, windowHeight);
  RConsole::Canvas::SetCursorVisible(false);
  Clear();

  while (true)
  {
    // Prepare
    Timepoint start = std::chrono::steady_clock::now();
    const double lastFrameMS = lastFrameMicroseconds / 1000.0;
    const double lastFrameS = lastFrameMS / 1000.0;
    ResizeIfNeeded();

    // Update
    parser.HandleInput(ProcessInputChar, ProcessInputString);
    flameParticles.Update(lastFrameS);
    HandlePendingScrapedData(fileParticles, data, lastFrameS);
    TryUpdate(fileParticles, lastFrameS);
    RConsole::Canvas::Update();

    // Draw
    DrawBackgroundLog();
    DrawParticles(flameParticles);
    DrawParticles(fileParticles);
    DrawForegroundLog();
    DrawFrameTime(displayFrameTime);
    DrawColorDisplay(displayColors);
    DrawBurnCount(displayBurnCount);

    // Resolve (Post-update)
    std::this_thread::yield(); // Be polite! 
    std::this_thread::sleep_until(start + std::chrono::milliseconds(4)); // Max speed of 
    Timepoint end = std::chrono::steady_clock::now();
    lastFrameMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  }

  delete fileParticles;
  return 0;
}

/// <summary>
/// Attempts to silently send the specified file or folder at the path to the recycle bin
/// </summary>
/// <param name="path"></param>
/// <returns>True if no issues, false if issues / could not delete.</returns>
bool TryRecyclePath(std::string path)
{
  // Construct and pad path. An extra bit of null won't hurt anything.
  const std::wstring end_coverage = std::wstring(1, L'\0');
  std::wstring wide_path = wstringConverter.from_bytes(path) + end_coverage;

  // Construct request
  SHFILEOPSTRUCT fileOp;
  fileOp.hwnd = NULL;
  fileOp.wFunc = FO_DELETE;
  fileOp.pFrom = wide_path.c_str();
  fileOp.pTo = NULL;
  fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOF_SILENT;

  // Pass request and result
  int result = SHFileOperation(&fileOp);
  return result == 0;
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
    //Clear();
    delete scrapeSys;
  }

  scrapeSys = new ParticleSystem<ParticleData>(sizeof(scraped), 0.003, false, data, CreateFileParticle, UpdateParticle);

  scrapedLocation = 0;
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
/// Displays the number of files burned
/// </summary>
/// <param name="is_displaying"></param>
void DrawBurnCount(bool is_displaying)
{
  if (numberBurned == 0)
  {
    return;
  }

  std::string pluralAppend = (numberBurned != 1 ? "s" : "");
  std::string toDraw = std::to_string(numberBurned) + " file" + pluralAppend + " burned";
  const int xPos = windowWidth / 2 - toDraw.length() / 2;
  const int yPos = 1;

  RConsole::Canvas::DrawString(toDraw, xPos, yPos, RConsole::Color::GREY);
}

/// <summary>
/// Consumes extended input
/// </summary>
/// <param name="path"></param>
void ProcessInputString(std::string path)
{
  bool pathNeedsQuotesStripped = path.length() > 2 && path[0] == '"' && path[path.length() - 1] == '"';
  if (pathNeedsQuotesStripped)
  {
    path = path.substr(1, path.length() - 2);
  }
  
  // Create our objects.
  std::ifstream fileObject(path, std::ios::binary);
  
  // See if we opened the file successfully.
  if (!fileObject.is_open())
  {
    // We can safely assume it's closed
    ++numberBurned;
    TryRecyclePath(path); // Could still be a folder, so go for it anyways.
    return;
  }

  fileObject.read(scraped, sizeof(scraped));
  pendingScrapeData = true;

  fileObject.close();
  ++numberBurned;
  TryRecyclePath(path);
}

/// <summary>
/// Consumes single item input
/// </summary>
void ProcessInputChar(char key)
{
  switch (key)
  {
    case 'b':
    case 'c':
      displayBurnCount = !displayBurnCount;
      break;

    case 'd':
    case 'f':
      displayFrameTime = !displayFrameTime;
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

  if (scrapedLocation >= sizeof(scraped))
  {
    throw "Attempting to walk off the end of the scraped list. Not good.";
  }

  char toShow = scraped[scrapedLocation];
  if (toShow == static_cast<unsigned char>(7))
  {
    toShow = '?';
  }

  p.Data.visual = toShow;
  ++scrapedLocation;
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
