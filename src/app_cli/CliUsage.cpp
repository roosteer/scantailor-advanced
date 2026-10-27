// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CliUsage.h"

#include <QTextStream>

#include <algorithm>

#include "FilterRange.h"
#include "config.h"
#include "version.h"

namespace cli {

namespace {

bool parseInt(const QString& str, int& out) {
  bool ok = false;
  out = str.toInt(&ok);
  return ok;
}

bool parseDouble(const QString& str, double& out) {
  bool ok = false;
  out = str.toDouble(&ok);
  return ok;
}

bool parseMmRect(const QString& str, MmRect& out) {
  const QStringList parts = str.split(',');
  if (parts.size() != 4) {
    return false;
  }
  return parseDouble(parts[0], out.left) && parseDouble(parts[1], out.top) && parseDouble(parts[2], out.right)
         && parseDouble(parts[3], out.bottom);
}

ExitCode reject(QString& errorMessage, const QString& message) {
  errorMessage = message;
  return ExitCode::INVALID_INPUT;
}

ExitCode setHelp(const QString&, CliRequest& out, QString&) {
  out.help = true;
  return ExitCode::SUCCESS;
}

ExitCode setVersion(const QString&, CliRequest& out, QString&) {
  out.version = true;
  return ExitCode::SUCCESS;
}

ExitCode setVerbose(const QString&, CliRequest& out, QString&) {
  out.verbose = true;
  return ExitCode::SUCCESS;
}

ExitCode setQuiet(const QString&, CliRequest& out, QString&) {
  out.quiet = true;
  return ExitCode::SUCCESS;
}

ExitCode setEqualizeIllumination(const QString&, CliRequest& out, QString&) {
  out.equalizeIllumination = true;
  return ExitCode::SUCCESS;
}

ExitCode setOutputFormat(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "text") {
    out.outputFormat = OutputFormat::TEXT;
  } else if (value == "json") {
    out.outputFormat = OutputFormat::JSON;
  } else {
    return reject(errorMessage, QStringLiteral("--output requires 'text' or 'json'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setOutputProject(const QString& value, CliRequest& out, QString&) {
  out.outputProjectFile = value;
  return ExitCode::SUCCESS;
}

ExitCode parseFilterIndex(const QString& flag,
                          const QString& value,
                          std::optional<int>& target,
                          QString& errorMessage) {
  int idx = 0;
  if (!parseInt(value, idx) || idx < 0 || idx >= kFilterCount) {
    return reject(errorMessage,
                  QString("%1 requires an integer 0-%2").arg(flag).arg(kFilterCount - 1));
  }
  target = idx;
  return ExitCode::SUCCESS;
}

ExitCode setStartFilter(const QString& value, CliRequest& out, QString& errorMessage) {
  return parseFilterIndex("--start-filter", value, out.startFilter, errorMessage);
}

ExitCode setEndFilter(const QString& value, CliRequest& out, QString& errorMessage) {
  return parseFilterIndex("--end-filter", value, out.endFilter, errorMessage);
}

ExitCode setOrientation(const QString& value, CliRequest& out, QString& errorMessage) {
  int degrees = 0;
  if (!parseInt(value, degrees)
      || (degrees != 0 && degrees != 90 && degrees != 180 && degrees != 270)) {
    return reject(errorMessage, QStringLiteral("--orientation requires 0, 90, 180, or 270"));
  }
  out.orientation = degrees;
  return ExitCode::SUCCESS;
}

ExitCode setLayout(const QString& value, CliRequest& out, QString& errorMessage) {
  int layout = 0;
  if (!parseInt(value, layout) || layout < 0 || layout > 3) {
    return reject(errorMessage,
                  QStringLiteral("--layout requires 0 (auto), 1 (single), 2 (1+offcut), or 3 (two)"));
  }
  out.layoutType = static_cast<page_split::LayoutType>(layout);
  return ExitCode::SUCCESS;
}

ExitCode setDeskewMode(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "auto") {
    out.deskewMode = DeskewMode::AUTO;
  } else if (value == "manual") {
    out.deskewMode = DeskewMode::MANUAL;
  } else if (value == "off") {
    out.deskewMode = DeskewMode::OFF;
  } else {
    return reject(errorMessage, QStringLiteral("--deskew requires 'auto', 'manual', or 'off'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setDeskewAngle(const QString& value, CliRequest& out, QString& errorMessage) {
  double degrees = 0;
  if (!parseDouble(value, degrees)) {
    return reject(errorMessage,
                  QStringLiteral("--deskew-angle requires a numeric value in degrees"));
  }
  out.deskewAngle = degrees;
  return ExitCode::SUCCESS;
}

ExitCode setContentDetection(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "auto") {
    out.contentDetection = ContentDetection::AUTO;
  } else if (value == "manual") {
    out.contentDetection = ContentDetection::MANUAL;
  } else if (value == "off") {
    out.contentDetection = ContentDetection::OFF;
  } else {
    return reject(errorMessage,
                  QStringLiteral("--content-detection requires 'auto', 'manual', or 'off'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setContentBox(const QString& value, CliRequest& out, QString& errorMessage) {
  MmRect box;
  if (!parseMmRect(value, box)) {
    return reject(errorMessage,
                  QStringLiteral("--content-box requires four comma-separated numbers (l,t,r,b in mm)"));
  }
  out.contentBox = box;
  return ExitCode::SUCCESS;
}

ExitCode setMargins(const QString& value, CliRequest& out, QString& errorMessage) {
  MmRect margins;
  if (!parseMmRect(value, margins)) {
    return reject(errorMessage,
                  QStringLiteral("--margins requires four comma-separated numbers (l,t,r,b in mm)"));
  }
  out.margins = margins;
  return ExitCode::SUCCESS;
}

ExitCode setAlignment(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "center") {
    out.alignment = Alignment::CENTER;
  } else if (value == "original") {
    out.alignment = Alignment::ORIGINAL;
  } else if (value == "auto") {
    out.alignment = Alignment::AUTO;
  } else {
    return reject(errorMessage, QStringLiteral("--alignment requires 'center', 'original', or 'auto'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setOutputDpi(const QString& value, CliRequest& out, QString& errorMessage) {
  int dpi = 0;
  if (!parseInt(value, dpi) || dpi < 72) {
    return reject(errorMessage, QStringLiteral("--output-dpi requires an integer >= 72"));
  }
  out.outputDpi = dpi;
  return ExitCode::SUCCESS;
}

ExitCode setColorMode(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "bw") {
    out.colorMode = output::BLACK_AND_WHITE;
  } else if (value == "color") {
    out.colorMode = output::COLOR_GRAYSCALE;
  } else if (value == "mixed") {
    out.colorMode = output::MIXED;
  } else {
    return reject(errorMessage, QStringLiteral("--color-mode requires 'bw', 'color', or 'mixed'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setThreshold(const QString& value, CliRequest& out, QString& errorMessage) {
  int adjustment = 0;
  if (!parseInt(value, adjustment) || adjustment < 0 || adjustment > 255) {
    return reject(errorMessage, QStringLiteral("--threshold requires an integer 0-255"));
  }
  out.thresholdAdjustment = adjustment;
  return ExitCode::SUCCESS;
}

ExitCode setDespeckle(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "off") {
    out.despeckleLevel = output::DESPECKLE_OFF;
  } else if (value == "cautious") {
    out.despeckleLevel = output::DESPECKLE_CAUTIOUS;
  } else if (value == "normal") {
    out.despeckleLevel = output::DESPECKLE_NORMAL;
  } else if (value == "aggressive") {
    out.despeckleLevel = output::DESPECKLE_AGGRESSIVE;
  } else {
    return reject(errorMessage,
                  QStringLiteral("--despeckle requires 'off', 'cautious', 'normal', or 'aggressive'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setDewarping(const QString& value, CliRequest& out, QString& errorMessage) {
  if (value == "off") {
    out.dewarpingMode = output::OFF;
  } else if (value == "auto") {
    out.dewarpingMode = output::AUTO;
  } else if (value == "manual") {
    out.dewarpingMode = output::MANUAL;
  } else if (value == "marginal") {
    out.dewarpingMode = output::MARGINAL;
  } else {
    return reject(errorMessage,
                  QStringLiteral("--dewarping requires 'off', 'auto', 'manual', or 'marginal'"));
  }
  return ExitCode::SUCCESS;
}

ExitCode setDepthPerception(const QString& value, CliRequest& out, QString& errorMessage) {
  double depth = 0;
  if (!parseDouble(value, depth) || depth < 1.0 || depth > 3.0) {
    return reject(errorMessage, QStringLiteral("--depth-perception requires a value between 1.0 and 3.0"));
  }
  out.depthPerception = depth;
  return ExitCode::SUCCESS;
}

const std::vector<FlagSpec> kFlagSpecs = {
    {"--help", nullptr, "General", "Show this help and exit", &setHelp},
    {"--version", nullptr, "General", "Show version and exit", &setVersion},
    {"--verbose", nullptr, "General", "Detailed progress to stderr", &setVerbose},
    {"--quiet", nullptr, "General", "Suppress progress output; errors are still printed", &setQuiet},
    {"--output", "text|json", "General", "Report format on stdout (default: text)", &setOutputFormat},
    {"--output-project", "file", "General", "Save a .ScanTailor project after processing", &setOutputProject},
    {"--start-filter", "N", "Filter selection",
     "Apply flag overrides from filter N onward (does not skip earlier stages)", &setStartFilter},
    {"--end-filter", "N", "Filter selection", "Stop the pipeline after filter N; later stages are skipped",
     &setEndFilter},
    {"--orientation", "deg", "fix_orientation", "Rotate: 0, 90, 180, 270", &setOrientation},
    {"--layout", "n", "page_split", "Page layout: 0=auto, 1=single, 2=1+offcut, 3=two", &setLayout},
    {"--deskew", "mode", "deskew", "Deskew: auto, manual, off", &setDeskewMode},
    {"--deskew-angle", "deg", "deskew", "Manual deskew angle (requires --deskew=manual)", &setDeskewAngle},
    {"--content-detection", "mode", "select_content", "Content detection: auto, manual, off",
     &setContentDetection},
    {"--content-box", "l,t,r,b", "select_content",
     "Manual content box in mm (requires --content-detection=manual)", &setContentBox},
    {"--margins", "l,t,r,b", "page_layout", "Hard margins in mm", &setMargins},
    {"--alignment", "mode", "page_layout", "Alignment: center, original, auto", &setAlignment},
    {"--output-dpi", "N", "output", "Output DPI (>= 72)", &setOutputDpi},
    {"--color-mode", "mode", "output", "Color mode: bw, color, mixed", &setColorMode},
    {"--threshold", "N", "output", "Binarization threshold adjustment (0-255, default 0)", &setThreshold},
    {"--despeckle", "level", "output", "Despeckle: off, cautious, normal, aggressive", &setDespeckle},
    {"--dewarping", "mode", "output", "Dewarping: off, auto, manual, marginal", &setDewarping},
    {"--depth-perception", "val", "output", "Dewarping depth perception (1.0-3.0)", &setDepthPerception},
    {"--equalize-illumination", nullptr, "output", "Normalize illumination before binarization",
     &setEqualizeIllumination},
};

QString flagText(const FlagSpec& spec) {
  QString text = QString::fromLatin1(spec.name);
  if (spec.valueHint != nullptr) {
    text += '=';
    text += QString::fromLatin1(spec.valueHint);
  }
  return text;
}

}  // namespace

const std::vector<FlagSpec>& flagSpecs() {
  return kFlagSpecs;
}

const FlagSpec* findFlagSpec(const QString& name) {
  for (const FlagSpec& spec : kFlagSpecs) {
    if (name == QLatin1String(spec.name)) {
      return &spec;
    }
  }
  return nullptr;
}

void printHelp() {
  QTextStream out(stdout);

  out << "Usage: scantailor-advanced-cli [flags] <images...> <output-dir>\n";
  out << "       scantailor-advanced-cli [flags] <project.ScanTailor>\n";
  out << "       scantailor-advanced-cli [flags] <project.ScanTailor> <output-dir>\n";
  out << "Process scanned images through the ScanTailor Advanced pipeline.\n";
  out << "\n";
  out << "Values are attached with '=': --output-dpi=300. A space-separated value is not accepted.\n";
  out << "\n";
  out << "Filter indices (for --start-filter / --end-filter):\n";
  out << "  0 = fix_orientation   1 = page_split   2 = deskew\n";
  out << "  3 = select_content    4 = page_layout  5 = output\n";
  out << "\n";

  int width = 0;
  for (const FlagSpec& spec : kFlagSpecs) {
    width = std::max(width, flagText(spec).size());
  }

  QString lastGroup;
  for (const FlagSpec& spec : kFlagSpecs) {
    const QString group = QString::fromLatin1(spec.group);
    if (group != lastGroup) {
      out << (lastGroup.isEmpty() ? "" : "\n") << group << ":\n";
      lastGroup = group;
    }
    out << "  " << flagText(spec).leftJustified(width) << "  " << QString::fromLatin1(spec.description) << "\n";
  }

  out << "\n";
  out << "Exit codes:\n";
  out << "  0  Success\n";
  out << "  2  Invalid input (bad flag, bad value)\n";
  out << "  3  File not found\n";
  out << "  4  Filter processing error\n";
  out << "  5  Output write error\n";
  out << "  6  Internal error\n";
  out << "\n";
  out << "Examples:\n";
  out << "  scantailor-advanced-cli scan.png /tmp/out\n";
  out << "  scantailor-advanced-cli --output-dpi=300 --color-mode=bw scan1.png scan2.png /tmp/out\n";
  out << "  scantailor-advanced-cli --output=json project.ScanTailor\n";
  out << "  scantailor-advanced-cli --output-dpi=600 project.ScanTailor /tmp/out\n";
}

void printVersion() {
  QTextStream out(stdout);
  out << "scantailor-advanced-cli " << VERSION << "\n";
  out << "ScanTailor Advanced " << VERSION << " (project version " << PROJECT_VERSION << ")\n";
}

}  // namespace cli
