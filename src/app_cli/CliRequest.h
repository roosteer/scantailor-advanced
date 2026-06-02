// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CLIREQUEST_H_
#define SCANTAILOR_APP_CLI_CLIREQUEST_H_

#include <QString>
#include <QStringList>

#include "ExitCode.h"
#include "filters/output/ColorParams.h"
#include "filters/output/DespeckleLevel.h"
#include "filters/output/DewarpingOptions.h"
#include "filters/page_split/LayoutType.h"

namespace cli {

/**
 * Value-type DTO representing fully parsed CLI arguments.
 * All fields are set to defaults when not specified by the user.
 */
struct CliRequest {
  /** Input image file paths (positional args). */
  QStringList images;

  /** Path to a .ScanTailor project file. */
  QString projectFile;

  /** Output directory. */
  QString outputDir;

  /** Path to save project after processing. */
  QString outputProjectFile;

  /** Output mode: "default" or "json". */
  QString outputMode;

  // --- Filter selection ---
  int startFilter = -1;  // -1 = not set (default: 0)
  int endFilter = -1;    // -1 = not set (default: 5)

  // --- fix_orientation ---
  /** Rotation in degrees: 0, 90, 180, 270. -1 = not set. */
  int orientation = -1;

  // --- page_split ---
  page_split::LayoutType layoutType = page_split::AUTO_LAYOUT_TYPE;
  bool layoutTypeSet = false;

  // --- deskew ---
  enum DeskewMode { DESKEW_AUTO, DESKEW_MANUAL, DESKEW_OFF };
  DeskewMode deskewMode = DESKEW_AUTO;
  bool deskewModeSet = false;
  double deskewAngle = 0.0;     // manual deskew angle in degrees
  bool deskewAngleSet = false;

  // --- select_content ---
  enum ContentDetection { CONTENT_AUTO, CONTENT_MANUAL, CONTENT_OFF };
  ContentDetection contentDetection = CONTENT_AUTO;
  bool contentDetectionSet = false;
  /** Content box: left,top,right,bottom in mm. All -1 = not set. */
  double contentBox[4] = {-1, -1, -1, -1};

  // --- page_layout ---
  /** Margins in mm: left, top, right, bottom. All -1 = not set. */
  double margins[4] = {-1, -1, -1, -1};
  enum Alignment { ALIGN_CENTER, ALIGN_ORIGINAL, ALIGN_AUTO };
  Alignment alignment = ALIGN_CENTER;
  bool alignmentSet = false;

  // --- output ---
  int outputDpi = -1;  // -1 = not set
  output::ColorMode colorMode = output::BLACK_AND_WHITE;
  bool colorModeSet = false;
  int threshold = -1;  // -1 = not set (black/white threshold)
  output::DespeckleLevel despeckleLevel = output::DESPECKLE_OFF;
  bool despeckleLevelSet = false;
  output::DewarpingMode dewarpingMode = output::OFF;
  bool dewarpingModeSet = false;
  double depthPerception = -1.0;  // -1 = not set
  bool whiteMargins = false;
  bool whiteMarginsSet = false;
  bool equalizeIllumination = false;
  bool equalizeIlluminationSet = false;
  bool savitzkyGolaySmoothing = false;
  bool savitzkyGolaySmoothingSet = false;
  bool morphologicalSmoothing = false;
  bool morphologicalSmoothingSet = false;

  // --- Misc ---
  bool help = false;
  bool version = false;
  bool verbose = false;
  bool quiet = false;

  /**
   * Parse argv (excluding argv[0]) into a CliRequest.
   * Returns the populated request and ExitCode::SUCCESS on success.
   * On error, returns a partial request and the appropriate exit code.
   */
  static ExitCode parse(const QStringList& args, CliRequest& out, QString& errorMsg);
};

/** Print usage help to stdout. */
void printHelp();

/** Print version info to stdout. */
void printVersion();

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CLIREQUEST_H_
