// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_CLI_CLIREQUEST_H_
#define SCANTAILOR_APP_CLI_CLIREQUEST_H_

#include <QString>
#include <QStringList>
#include <optional>

#include "filters/output/ColorParams.h"
#include "filters/output/DespeckleLevel.h"
#include "filters/output/DewarpingOptions.h"
#include "filters/page_split/LayoutType.h"

namespace cli {

enum class OutputFormat { TEXT, JSON };

enum class DeskewMode { AUTO, MANUAL, OFF };

enum class ContentDetection { AUTO, MANUAL, OFF };

enum class Alignment { CENTER, ORIGINAL, AUTO };

/** A rectangle in millimetres, as printed on the page. */
struct MmRect {
  double left = 0;
  double top = 0;
  double right = 0;
  double bottom = 0;
};

/**
 * Fully parsed command line. Every option the user did not pass stays disengaged,
 * so consumers ask "did the user say anything about this?" instead of comparing
 * against a sentinel.
 */
struct CliRequest {
  /** Input image file paths (positional arguments). */
  QStringList images;

  /** Path to a .ScanTailor project file. */
  QString projectFile;

  /** Output directory. */
  QString outputDir;

  /** Where to save the project after processing. */
  QString outputProjectFile;

  OutputFormat outputFormat = OutputFormat::TEXT;

  bool verbose = false;
  bool quiet = false;
  bool help = false;
  bool version = false;

  std::optional<int> startFilter;
  std::optional<int> endFilter;

  std::optional<int> orientation;
  std::optional<page_split::LayoutType> layoutType;

  std::optional<DeskewMode> deskewMode;
  std::optional<double> deskewAngle;

  std::optional<ContentDetection> contentDetection;
  std::optional<MmRect> contentBox;

  std::optional<MmRect> margins;
  std::optional<Alignment> alignment;

  std::optional<int> outputDpi;
  std::optional<output::ColorMode> colorMode;
  std::optional<int> thresholdAdjustment;
  std::optional<output::DespeckleLevel> despeckleLevel;

  std::optional<output::DewarpingMode> dewarpingMode;
  std::optional<double> depthPerception;

  bool equalizeIllumination = false;
};

}  // namespace cli

#endif  // SCANTAILOR_APP_CLI_CLIREQUEST_H_
