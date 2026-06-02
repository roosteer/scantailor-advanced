// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "CliRequest.h"

#include <QCoreApplication>
#include <QTextStream>

#include "config.h"
#include "version.h"

namespace cli {

void printHelp() {
  QTextStream out(stdout);
  out << "Usage: scantailor-advanced-cli [flags] <images...> <output-dir>\n";
  out << "       scantailor-advanced-cli [flags] <project.ScanTailor>\n";
  out << "\n";
  out << "Process scanned images through the ScanTailor Advanced pipeline.\n";
  out << "\n";
  out << "Filter indices (for --start-filter / --end-filter):\n";
  out << "  0 = fix_orientation  1 = page_split  2 = deskew\n";
  out << "  3 = select_content   4 = page_layout  5 = output\n";
  out << "\n";
  out << "Flags:\n";
  out << "  --help                     Show this help and exit\n";
  out << "  --version                  Show version and exit\n";
  out << "  --verbose                  Detailed progress to stderr\n";
  out << "  --quiet                    Suppress all non-error output\n";
  out << "  --output=json              Produce JSON report on stdout\n";
  out << "  --output-project=<file>    Save .ScanTailor project after processing\n";
  out << "  --start-filter=N           Start at filter index N (0-based)\n";
  out << "  --end-filter=N             Stop after filter index N (0-based)\n";
  out << "\n";
  out << "  --orientation=<deg>        Rotate: 0, 90, 180, 270\n";
  out << "  --layout=<n>               Page layout: 0=auto, 1=single, 2=1+offcut, 3=two\n";
  out << "\n";
  out << "  --deskew=<mode>            Deskew: auto, manual, off\n";
  out << "  --deskew-angle=<deg>       Manual deskew angle in degrees\n";
  out << "\n";
  out << "  --content-detection=<mode> Content detection: auto, manual, off\n";
  out << "  --content-box=l,t,r,b      Manual content box in mm\n";
  out << "\n";
  out << "  --margins=l,t,r,b          Hard margins in mm\n";
  out << "  --alignment=<mode>         Alignment: center, original, auto\n";
  out << "\n";
  out << "  --output-dpi=N             Output DPI\n";
  out << "  --color-mode=<mode>        Color mode: bw, color, mixed\n";
  out << "  --threshold=N              Black/white threshold (0-255, default 43)\n";
  out << "  --despeckle=<level>        Despeckle: off, cautious, normal, aggressive\n";
  out << "  --dewarping=<mode>         Dewarping: off, auto, manual, marginal\n";
  out << "  --depth-perception=<val>   Dewarping depth perception (1.0-3.0)\n";
  out << "  --white-margins            Enable white margins\n";
  out << "  --equalize-illumination    Enable illumination equalization\n";
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
}

void printVersion() {
  QTextStream out(stdout);
  out << "scantailor-advanced-cli " << VERSION << "\n";
  out << "ScanTailor Advanced " << VERSION << " (project version " << PROJECT_VERSION << ")\n";
}

// --- Helpers ---

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

bool parseMargins(const QString& str, double margins[4]) {
  const QStringList parts = str.split(',');
  if (parts.size() != 4) {
    return false;
  }
  return parseDouble(parts[0], margins[0]) && parseDouble(parts[1], margins[1])
         && parseDouble(parts[2], margins[2]) && parseDouble(parts[3], margins[3]);
}

ExitCode parseFlag(const QString& arg, CliRequest& out, QString& errorMsg) {
  // arg is expected to look like "--flag" or "--flag=value"
  const int eqIdx = arg.indexOf('=');
  const QString name = (eqIdx >= 0) ? arg.left(eqIdx) : arg;
  const QString value = (eqIdx >= 0) ? arg.mid(eqIdx + 1) : QString();

  if (name == "--help") {
    out.help = true;
  } else if (name == "--version") {
    out.version = true;
  } else if (name == "--verbose") {
    out.verbose = true;
  } else if (name == "--quiet") {
    out.quiet = true;
  } else if (name == "--output") {
    if (value.isEmpty()) {
      errorMsg = QString("--output requires a value (e.g. --output=json)");
      return ExitCode::INVALID_INPUT;
    }
    out.outputMode = value;
  } else if (name == "--output-project") {
    if (value.isEmpty()) {
      errorMsg = QString("--output-project requires a file path");
      return ExitCode::INVALID_INPUT;
    }
    out.outputProjectFile = value;
  } else if (name == "--start-filter") {
    if (!parseInt(value, out.startFilter) || out.startFilter < 0 || out.startFilter > 5) {
      errorMsg = QString("--start-filter requires an integer 0-5");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--end-filter") {
    if (!parseInt(value, out.endFilter) || out.endFilter < 0 || out.endFilter > 5) {
      errorMsg = QString("--end-filter requires an integer 0-5");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--orientation") {
    if (!parseInt(value, out.orientation) || (out.orientation != 0 && out.orientation != 90
                                              && out.orientation != 180 && out.orientation != 270)) {
      errorMsg = QString("--orientation requires 0, 90, 180, or 270");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--layout") {
    int lv = 0;
    if (!parseInt(value, lv) || lv < 0 || lv > 3) {
      errorMsg = QString("--layout requires 0 (auto), 1 (single), 2 (1+offcut), or 3 (two)");
      return ExitCode::INVALID_INPUT;
    }
    out.layoutType = static_cast<page_split::LayoutType>(lv);
    out.layoutTypeSet = true;
  } else if (name == "--deskew") {
    if (value == "auto") {
      out.deskewMode = CliRequest::DESKEW_AUTO;
    } else if (value == "manual") {
      out.deskewMode = CliRequest::DESKEW_MANUAL;
    } else if (value == "off") {
      out.deskewMode = CliRequest::DESKEW_OFF;
    } else {
      errorMsg = QString("--deskew requires 'auto', 'manual', or 'off'");
      return ExitCode::INVALID_INPUT;
    }
    out.deskewModeSet = true;
  } else if (name == "--deskew-angle") {
    if (!parseDouble(value, out.deskewAngle)) {
      errorMsg = QString("--deskew-angle requires a numeric value");
      return ExitCode::INVALID_INPUT;
    }
    out.deskewAngleSet = true;
  } else if (name == "--content-detection") {
    if (value == "auto") {
      out.contentDetection = CliRequest::CONTENT_AUTO;
    } else if (value == "manual") {
      out.contentDetection = CliRequest::CONTENT_MANUAL;
    } else if (value == "off") {
      out.contentDetection = CliRequest::CONTENT_OFF;
    } else {
      errorMsg = QString("--content-detection requires 'auto', 'manual', or 'off'");
      return ExitCode::INVALID_INPUT;
    }
    out.contentDetectionSet = true;
  } else if (name == "--content-box") {
    if (!parseMargins(value, out.contentBox)) {
      errorMsg = QString("--content-box requires four comma-separated numbers (l,t,r,b in mm)");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--margins") {
    if (!parseMargins(value, out.margins)) {
      errorMsg = QString("--margins requires four comma-separated numbers (l,t,r,b in mm)");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--alignment") {
    if (value == "center") {
      out.alignment = CliRequest::ALIGN_CENTER;
    } else if (value == "original") {
      out.alignment = CliRequest::ALIGN_ORIGINAL;
    } else if (value == "auto") {
      out.alignment = CliRequest::ALIGN_AUTO;
    } else {
      errorMsg = QString("--alignment requires 'center', 'original', or 'auto'");
      return ExitCode::INVALID_INPUT;
    }
    out.alignmentSet = true;
  } else if (name == "--output-dpi") {
    if (!parseInt(value, out.outputDpi) || out.outputDpi < 72) {
      errorMsg = QString("--output-dpi requires an integer >= 72");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--color-mode") {
    if (value == "bw" || value == "black_and_white") {
      out.colorMode = output::BLACK_AND_WHITE;
    } else if (value == "color" || value == "color_grayscale") {
      out.colorMode = output::COLOR_GRAYSCALE;
    } else if (value == "mixed") {
      out.colorMode = output::MIXED;
    } else {
      errorMsg = QString("--color-mode requires 'bw', 'color', or 'mixed'");
      return ExitCode::INVALID_INPUT;
    }
    out.colorModeSet = true;
  } else if (name == "--threshold") {
    if (!parseInt(value, out.threshold) || out.threshold < 0 || out.threshold > 255) {
      errorMsg = QString("--threshold requires an integer 0-255");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--despeckle") {
    if (value == "off") {
      out.despeckleLevel = output::DESPECKLE_OFF;
    } else if (value == "cautious") {
      out.despeckleLevel = output::DESPECKLE_CAUTIOUS;
    } else if (value == "normal") {
      out.despeckleLevel = output::DESPECKLE_NORMAL;
    } else if (value == "aggressive") {
      out.despeckleLevel = output::DESPECKLE_AGGRESSIVE;
    } else {
      errorMsg = QString("--despeckle requires 'off', 'cautious', 'normal', or 'aggressive'");
      return ExitCode::INVALID_INPUT;
    }
    out.despeckleLevelSet = true;
  } else if (name == "--dewarping") {
    if (value == "off") {
      out.dewarpingMode = output::OFF;
    } else if (value == "auto") {
      out.dewarpingMode = output::AUTO;
    } else if (value == "manual") {
      out.dewarpingMode = output::MANUAL;
    } else if (value == "marginal") {
      out.dewarpingMode = output::MARGINAL;
    } else {
      errorMsg = QString("--dewarping requires 'off', 'auto', 'manual', or 'marginal'");
      return ExitCode::INVALID_INPUT;
    }
    out.dewarpingModeSet = true;
  } else if (name == "--depth-perception") {
    if (!parseDouble(value, out.depthPerception) || out.depthPerception < 1.0 || out.depthPerception > 3.0) {
      errorMsg = QString("--depth-perception requires a value between 1.0 and 3.0");
      return ExitCode::INVALID_INPUT;
    }
  } else if (name == "--white-margins") {
    out.whiteMargins = true;
    out.whiteMarginsSet = true;
  } else if (name == "--equalize-illumination") {
    out.equalizeIllumination = true;
    out.equalizeIlluminationSet = true;
  } else {
    errorMsg = QString("Unknown flag: %1").arg(name);
    return ExitCode::INVALID_INPUT;
  }

  return ExitCode::SUCCESS;
}

}  // anonymous namespace

ExitCode CliRequest::parse(const QStringList& args, CliRequest& out, QString& errorMsg) {
  out = CliRequest();

  QStringList positionalArgs;

  for (const QString& arg : args) {
    if (arg.startsWith("--")) {
      const ExitCode ec = parseFlag(arg, out, errorMsg);
      if (ec != ExitCode::SUCCESS) {
        return ec;
      }
    } else {
      positionalArgs << arg;
    }
  }

  // Handle early exits: --help and --version don't need positional args.
  if (out.help || out.version) {
    return ExitCode::SUCCESS;
  }

  // Determine if this is a project file or image list.
  if (positionalArgs.isEmpty()) {
    errorMsg = QString("No input files specified. Use --help for usage.");
    return ExitCode::INVALID_INPUT;
  }

  // Check if the last positional arg looks like a directory path or project file.
  // Heuristic: if exactly 1 arg and it ends with .ScanTailor, treat as project file.
  // If exactly 1 arg that does NOT end with .ScanTailor, it's ambiguous — treat as output dir
  //   with no images, which is an error.
  // If N>=2 args, the last is output dir, the rest are images.
  if (positionalArgs.size() == 1 && positionalArgs[0].endsWith(".ScanTailor", Qt::CaseInsensitive)) {
    out.projectFile = positionalArgs[0];
  } else if (positionalArgs.size() >= 2) {
    out.outputDir = positionalArgs.takeLast();
    out.images = positionalArgs;
  } else {
    errorMsg = QString("Expected images and output directory, or a .ScanTailor project file. Use --help for usage.");
    return ExitCode::INVALID_INPUT;
  }

  return ExitCode::SUCCESS;
}

}  // namespace cli
