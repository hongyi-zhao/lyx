# file legacy_lyxpreview2ppm.py
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Angus Leeming
# Full author contact details are available in file CREDITS

# with much advice from members of the preview-latex project:
#   David Kastrup, dak@gnu.org and
#   Jan-Åke Larsson, jalar@mai.liu.se.
# and with much help testing the code under Windows from
#   Paul A. Rubin, rubin@msu.edu.

# This script takes a LaTeX file and generates a collection of
# png or ppm image files, one per previewed snippet.
# Example usage:
# legacy_lyxpreview2bitmap.py 0lyxpreview.tex 128 ppm 000000 faf0e6

# This script takes five arguments:
# TEXFILE:       the name of the .tex file to be converted.
# SCALEFACTOR:   a scale factor, used to ascertain the resolution of the
#                generated image which is then passed to gs.
# OUTPUTFORMAT:  the format of the output bitmap image files.
#                This particular script can produce only "ppm" format output.
# FG_COLOR:      the foreground color as a hexadecimal string, eg '000000'.
# BG_COLOR:      the background color as a hexadecimal string, eg 'faf0e6'.

# Decomposing TEXFILE's name as DIR/BASE.tex, this script will,
# if executed successfully, leave in DIR:
# * a (possibly large) number of image files with names
#   like BASE[0-9]+.(ppm|png)
# * a file BASE.metrics, containing info needed by LyX to position
#   the images correctly on the screen.

# The script uses several external programs and files:
# * python 2.4 or later (subprocess module);
# * A latex executable;
# * preview.sty;
# * dvips;
# * gs;
# * pdflatex (optional);
# * pnmcrop (optional).
# * pdftocairo (optional).
# * epstopdf (optional).

# preview.sty is part of the preview-latex project
#   http://preview-latex.sourceforge.net/
# Alternatively, it can be obtained from
#   CTAN/support/preview-latex/

# What does this script do?
# [legacy_conversion]
# 0) Process command-line arguments
# [legacy_conversion_step1]
# 1) Call latex to create a DVI file from LaTeX
# [legacy_conversion_step2]
# 2) Call dvips to create one PS file for each DVI page
# [legacy_conversion_step3]
# 3) If dvips fails look for PDF and call pdftocairo or gs to produce bitmaps
# 4) Otherwise call pdftocairo or gs on each PostScript file to produce bitmaps
# [legacy_conversion_pdflatex]
# 5) Keep track of pages on which gs failed and pass them to pdflatex
# 6) Call pdftocairo or gs on the PDF output from pdflatex to produce bitmaps
# 7) Extract and write to file (or return to lyxpreview2bitmap)
#    metrics from both methods (standard and pdflatex)

# The script uses the old dvi->ps->png conversion route,
# which is good when using PSTricks, TikZ or other packages involving
# PostScript literals (steps 1, 2, 4).
# This script also generates bitmaps from PDF created by a call to
# lyxpreview2bitmap.py passing "pdflatex" to the CONVERTER parameter
# (step 3).
# Finally, there's also has a fallback method based on pdflatex, which
# is required in certain cases, if hyperref is active for instance,
# (step 5, 6).
# If possible, dvipng should be used, as it's much faster.
# If possible, the script will use pdftocairo instead of gs,
# as it's much faster and gives better results.

import glob, os, re, subprocess, sys, tempfile

from lyxpreview_tools import check_latex_log, copyfileobj, error, filter_pages,\
     find_exe, find_exe_or_terminate, join_metrics_and_rename, latex_commands, \
     latex_file_re, make_texcolor, pdflatex_commands, progress, \
     run_command, run_latex, warning, write_metrics_info


def usage(prog_name):
    return "Usage: %s <latex file> <dpi> ppm <fg color> <bg color>\n" \
           "\twhere the colors are hexadecimal strings, eg 'faf0e6'" \
           % prog_name

# Returns a list of tuples containing page number and ascent fraction
# extracted from dvipng output.
# Use write_metrics_info to create the .metrics file with this info
def legacy_extract_metrics_info(log_file):

    log_re = re.compile(b"Preview: ([ST])")
    data_re = re.compile(b"(-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)")

    tp_ascent  = 0.0
    tp_descent = 0.0

    success = 0
    results = []
    try:
        for line in open(log_file, 'rb').readlines():
            match = log_re.match(line)
            if match == None:
                continue

            snippet = (match.group(1) == b'S')
            success = 1
            match = data_re.search(line)
            if match == None:
                error(f"Unexpected data in {log_file}\n{line}")

            if snippet:
                ascent  = float(match.group(2))
                descent = float(match.group(3))

                frac = 0.5
                if ascent == 0 and descent == 0:
                    # This is an empty image, forbid its display
                    frac = -1.0
                elif ascent >= 0 or descent >= 0:
                    ascent = ascent + tp_ascent
                    descent = descent - tp_descent

                    if abs(ascent + descent) > 0.1:
                        frac = ascent / (ascent + descent)

                    # Sanity check
                    if frac < 0 or frac > 1:
                            frac = 0.5

                results.append((int(match.group(1)), frac))

            else:
                tp_descent = float(match.group(2))
                tp_ascent  = float(match.group(4))

    except:
        # Unable to open the file, but do nothing here because
        # the calling function will act on the value of 'success'.
        warning(f'Warning in legacy_extract_metrics_info! Unable to open "{log_file}"')
        warning(repr(sys.exc_info()[0]) + ',' + repr(sys.exc_info()[1]))

    if success == 0:
        error(f"Failed to extract metrics info from {log_file}")

    return results

def extract_resolution(log_file, dpi):
    fontsize_re = re.compile(b"Preview: Fontsize")
    magnification_re = re.compile(b"Preview: Magnification")
    extract_decimal_re = re.compile(br"([0-9\.]+)")
    extract_integer_re = re.compile(b"([0-9]+)")

    found_fontsize = 0
    found_magnification = 0

    # Default values
    magnification = 1000.0
    fontsize = 10.0

    try:
        for line in open(log_file, 'rb').readlines():
            if found_fontsize and found_magnification:
                break

            if not found_fontsize:
                match = fontsize_re.match(line)
                if match != None:
                    match = extract_decimal_re.search(line)
                    if match == None:
                        error(f"Unable to parse: {line}")
                    fontsize = float(match.group(1))
                    found_fontsize = 1
                    continue

            if not found_magnification:
                match = magnification_re.match(line)
                if match != None:
                    match = extract_integer_re.search(line)
                    if match == None:
                        error("Unable to parse: %s" % line)
                    magnification = float(match.group(1))
                    found_magnification = 1
                    continue

    except:
        warning('Warning in extract_resolution! Unable to open "%s"' % log_file)
        warning(repr(sys.exc_info()[0]) + ',' + repr(sys.exc_info()[1]))

    # This is safe because both fontsize and magnification have
    # non-zero default values.
    return dpi * (10.0 / fontsize) * (1000.0 / magnification)


def legacy_latex_file(latex_file, fg_color, bg_color):
    use_polyglossia_re = re.compile(b"\\s*\\\\usepackage{polyglossia}")
    use_preview_re = re.compile(b"\\s*\\\\usepackage\\[([^]]+)\\]{preview}")
    fg_color_gr = make_texcolor(fg_color, True)
    bg_color_gr = make_texcolor(bg_color, True)

    tmp = tempfile.TemporaryFile()

    success = 0
    try:
        f = open(latex_file, 'rb')
    except:
        # Unable to open the file, but do nothing here because
        # the calling function will act on the value of 'success'.
        warning('Warning in legacy_latex_file! Unable to open "%s"' % latex_file)
        warning(repr(sys.exc_info()[0]) + ',' + repr(sys.exc_info()[1]))

    polyglossia = False
    for line in f.readlines():
        if success:
            tmp.write(line)
            continue
        match = use_preview_re.match(line)
        polymatch = use_polyglossia_re.match(line)
        # Package order:
        # * if polyglossia is used, we need to load color before that
        #   (also, we do not have to load lmodern)
        # * else, color should be loaded before preview
        if match == None:
            if polymatch == None:
                tmp.write(line)
                continue
            else:
                tmp.write(b"""
\\usepackage{color}
\\definecolor{lyxfg}{rgb}{%s}
\\definecolor{lyxbg}{rgb}{%s}
\\pagecolor{lyxbg}
\\usepackage{polyglossia}
""" % (fg_color_gr, bg_color_gr))
                polyglossia = True
                continue
        success = 1
        # Preview options: add the options lyx and tightpage
        previewopts = match.group(1)
        if not polyglossia:
            tmp.write(b"""
\\usepackage{color}
\\definecolor{lyxfg}{rgb}{%s}
\\definecolor{lyxbg}{rgb}{%s}
\\pagecolor{lyxbg}
\\makeatletter
\\def\\@tempa{cmr}
\\ifx\\f@family\\@tempa
  \\IfFileExists{lmodern.sty}{\\usepackage{lmodern}}{\\usepackage{ae,aecompl}}
\\fi
""" % (fg_color_gr, bg_color_gr))
        tmp.write(b"""
\\usepackage[%s,tightpage]{preview}
\\makeatletter
\\ifdefined\\AddToHook
  \\AddToHook{env/preview/before}{\\leavevmode\\begingroup\\color{lyxbg}\\special{background \\current@color}\\special{ps::clippath fill}}
  \\AddToHook{env/preview/after}{\\endgroup}
\\else
  \\g@addto@macro\\preview{\\leavevmode\\begingroup\\color{lyxbg}\\special{background \\current@color}\\special{ps::clippath fill}}
  \\g@addto@macro\\endpreview{\\endgroup}
\\fi
\\g@addto@macro\\preview{\\color{lyxfg}}
\\let\\pr@set@pagerightoffset\\@empty
\\ifx\\pagerightoffset\\@undefined\\else
  \\def\\pr@set@pagerightoffset{\\ifnum\\pagedirection=1
      \\pagerightoffset=-1in
      \\advance\\pagerightoffset-\\pr@bb@i
      \\advance\\pagerightoffset\\pr@bb@iii
    \\fi
  }
\\fi
\\g@addto@macro\\pr@ship@end{\\pr@set@pagerightoffset}
\\makeatother
""" % previewopts)
    if success:
        copyfileobj(tmp, open(latex_file,"wb"), 1)

    return success


def crop_files(pnmcrop, basename):
    for file in glob.glob("%s*.ppm" % basename):
        tmp = tempfile.TemporaryFile()
        conv_call = f'{pnmcrop} -left -right -- {file}'
        conv_status = subprocess.run(conv_call, stdout=tmp)

        if conv_status:
            continue

        copyfileobj(tmp, open(file, "wb"), 1)


def legacy_conversion(argv, skipMetrics = False):
    # Parse and manipulate the command line arguments.
    if len(argv) == 7:
        latex = [argv[6]]
    elif len(argv) != 6:
        error(usage(argv[0]))
    else:
        latex = None

    dir, latex_file = os.path.split(argv[1])
    if len(dir) != 0:
        os.chdir(dir)

    dpi = int(argv[2])

    output_format = argv[3]

    fg_color = argv[4]
    bg_color = argv[5]

    # External programs used by the script.
    latex = find_exe_or_terminate(latex or latex_commands)

    pdf_output = latex in pdflatex_commands

    return legacy_conversion_step1(latex_file, dpi, output_format, fg_color,
        bg_color, latex, pdf_output, skipMetrics)


# Add color info to the latex file, since ghostscript doesn't
# have the option to set foreground and background colors on
# the command line. Run the resulting file through latex.
def legacy_conversion_step1(latex_file, dpi, output_format, fg_color, bg_color,
                            latex, pdf_output = False, skipMetrics = False):

    # Move color information, lyx and tightpage options into the latex file.
    if not legacy_latex_file(latex_file, fg_color, bg_color):
        error("""Unable to move the color information, and the lyx and tightpage
            options of preview-latex, into the latex file""")

    # Compile the latex file.
    latex_status, latex_stdout = run_latex(latex, latex_file)
    if latex_status:
        progress("Will try to recover from %s failure" % latex)

    if pdf_output:
        return legacy_conversion_step3(latex_file, dpi, output_format, True, skipMetrics)
    else:
        return legacy_conversion_step2(latex_file, dpi, output_format, skipMetrics)

# Creates a new LaTeX file from the original with pages specified in
# failed_pages, pass it through pdflatex and updates the metrics
# from the standard legacy route
def legacy_conversion_pdflatex(latex_file, failed_pages, legacy_metrics,
    use_pdftocairo, conv, gs_device, gs_ext, alpha, resolution, output_format):

    error_count = 0

    # Search for pdflatex executable
    pdflatex = find_exe(["pdflatex"])
    if pdflatex == None:
        warning("Can't find pdflatex. Some pages failed with all the possible routes.")
        failed_pages = []
    else:
        # Create a new LaTeX file from the original but only with failed pages
        pdf_latex_file = latex_file_re.sub("_pdflatex.tex", latex_file)
        filter_pages(latex_file, pdf_latex_file, failed_pages)

        # pdflatex call
        error_pages = []
        pdflatex_status, pdflatex_stdout = run_latex(pdflatex, pdf_latex_file)
        if pdflatex_status:
            error_pages = check_latex_log(latex_file_re.sub(".log", pdf_latex_file))

        pdf_file = latex_file_re.sub(".pdf", pdf_latex_file)
        latex_file_root = latex_file_re.sub("", pdf_latex_file)

        # Converter call to produce bitmaps
        if use_pdftocairo:
            conv_call = '%s -png -transp -r %d "%s" "%s"' \
                        % (conv, resolution, pdf_file, latex_file_root)
            conv_status, conv_stdout = run_command(conv_call)
            if not conv_status:
                seqnum_re = re.compile("-([0-9]+)")
                for name in glob.glob("%s-*.png" % latex_file_root):
                    match = seqnum_re.search(name)
                    if match != None:
                        new_name = seqnum_re.sub(str(int(match.group(1))), name)
                        os.rename(name, new_name)
        else:
            conv_call = '%s -dNOPAUSE -dBATCH -dSAFER -sDEVICE=%s ' \
                        '-sOutputFile="%s%%d.%s" ' \
                        '-dGraphicsAlphaBit=%d -dTextAlphaBits=%d ' \
                        '-r%f "%s"' \
                        % (conv, gs_device, latex_file_root, \
                            gs_ext, alpha, alpha, resolution, pdf_file)
            conv_status, conv_stdout = run_command(conv_call)

        if conv_status:
            # Give up!
            warning("Some pages failed with all the possible routes")
            failed_pages = []
        else:
            # We've done it!
            pdf_log_file = latex_file_re.sub(".log", pdf_latex_file)
            pdf_metrics = legacy_extract_metrics_info(pdf_log_file)

            # Invalidate metrics for pages that produced errors
            if len(error_pages) > 0:
                for index in error_pages:
                    pdf_metrics.pop(index - 1)
                    pdf_metrics.insert(index - 1, (index, -1.0))
                    error_count += 1

            original_bitmap = latex_file_re.sub("%d." + output_format, pdf_latex_file)
            destination_bitmap = latex_file_re.sub("%d." + output_format, latex_file)

            # Join the metrics with the those from dvips and rename the bitmap images
            join_metrics_and_rename(legacy_metrics, pdf_metrics, failed_pages,
                original_bitmap, destination_bitmap)

    return error_count


# The file has been processed through latex and we expect dvi output.
# Run dvips, taking note whether it was successful.
def legacy_conversion_step2(latex_file, dpi, output_format, skipMetrics = False):
    # External programs used by the script.
    dvips   = find_exe_or_terminate(["dvips"])

    # Run the dvi file through dvips.
    dvi_file = latex_file_re.sub(".dvi", latex_file)
    ps_file  = latex_file_re.sub(".ps",  latex_file)

    dvips_call = f'{dvips} -i -o "{ps_file}" "{dvi_file}"'
    dvips_failed = False

    dvips_status, dvips_stdout = run_command(dvips_call)
    if dvips_status:
        warning(f'Failed: {os.path.basename(dvips)} {dvi_file} ... looking for PDF')
        dvips_failed = True

    return legacy_conversion_step3(latex_file, dpi, output_format, dvips_failed, skipMetrics)


# Either latex and dvips have been run and we have a ps file, or
# pdflatex has been run and we have a pdf file. Proceed with pdftocairo or gs.
def legacy_conversion_step3(latex_file, dpi, output_format, dvips_failed, skipMetrics = False):
    # External programs used by the script.
    gs      = find_exe_or_terminate(["gswin32c", "gswin64c", "gs"])
    pnmcrop = find_exe(["pnmcrop"])
    pdftocairo = find_exe(["pdftocairo"])
    epstopdf   = find_exe(["epstopdf"])
    use_pdftocairo = pdftocairo != None and output_format == "png"
    if use_pdftocairo and os.name == 'nt':
        # On Windows, check for png support (see #10718)
        conv_status, conv_stdout = run_command("%s --help" % pdftocairo)
        use_pdftocairo = '-png' in conv_stdout
    if use_pdftocairo:
        conv = pdftocairo
    else:
        conv = gs

    # Files to process
    pdf_file  = latex_file_re.sub(".pdf", latex_file)
    ps_file  = latex_file_re.sub(".ps",  latex_file)

    # The latex file name without extension
    latex_file_root = latex_file_re.sub("", latex_file)

    # Extract resolution data for the converter from the log file.
    log_file = latex_file_re.sub(".log", latex_file)
    resolution = extract_resolution(log_file, dpi)

    # Check whether some pages produced errors
    error_pages = check_latex_log(log_file)

    # Older versions of gs have problems with a large degree of
    # anti-aliasing at high resolutions
    alpha = 4
    if resolution > 150:
        alpha = 2

    gs_device = "png16m"
    gs_ext = "png"
    if output_format == "ppm":
        gs_device = "pnmraw"
        gs_ext = "ppm"

    # Extract the metrics from the log file
    legacy_metrics = legacy_extract_metrics_info(log_file)

    # List of pages which failed to produce a correct output
    failed_pages = []

    # Generate the bitmap images
    if dvips_failed:
        # dvips failed, maybe there's a PDF, try to produce bitmaps
        if use_pdftocairo:
            conv_call = '%s -png -transp -r %d "%s" "%s"' \
                        % (pdftocairo, resolution, pdf_file, latex_file_root)

            conv_status, conv_stdout = run_command(conv_call)
            if not conv_status:
                seqnum_re = re.compile("-([0-9]+)")
                for name in glob.glob("%s-*.png" % latex_file_root):
                    match = seqnum_re.search(name)
                    if match != None:
                        new_name = seqnum_re.sub(str(int(match.group(1))), name)
                        os.rename(name, new_name)
        else:
            conv_call = '%s -dNOPAUSE -dBATCH -dSAFER -sDEVICE=%s ' \
                      '-sOutputFile="%s%%d.%s" ' \
                      '-dGraphicsAlphaBit=%d -dTextAlphaBits=%d ' \
                      '-r%f "%s"' \
                      % (gs, gs_device, latex_file_root, \
                         gs_ext, alpha, alpha, resolution, pdf_file)

            conv_status, conv_stdout = run_command(conv_call)

        if conv_status:
            error(f"Failed: {os.path.basename(conv)} {pdf_file}")
    else:
        # Model for calling the converter on each file
        if use_pdftocairo and epstopdf != None:
            conv_call = '%s -png -transp -singlefile -r %d "%%s" "%s%%d"' \
                        % (pdftocairo, resolution, latex_file_root)
        else:
            conv_call = '%s -dNOPAUSE -dBATCH -dSAFER -sDEVICE=%s ' \
                        '-sOutputFile="%s%%d.%s" ' \
                        '-dGraphicsAlphaBit=%d -dTextAlphaBits=%d ' \
                        '-r%f "%%s"' \
                        % (gs, gs_device, latex_file_root, \
                           gs_ext, alpha, alpha, resolution)

        i = 0
        # Collect all the PostScript files (like *.001, *.002, ...)
        ps_files = glob.glob("%s.[0-9][0-9][0-9]" % latex_file_root)
        ps_files.sort()

        # Call the converter for each file
        for file in ps_files:
            i = i + 1
            progress(f"Processing page {i}, file {file}")
            if use_pdftocairo and epstopdf != None:
                conv_name = "epstopdf"
                conv_status, conv_stdout = run_command(f"{epstopdf} --outfile={file}.pdf {file}")
                if not conv_status:
                    conv_name = "pdftocairo"
                    file = file + ".pdf"
                    conv_status, conv_stdout = run_command(conv_call % (file, i))
            else:
                conv_name = "ghostscript"
                conv_status, conv_stdout = run_command(conv_call % (i, file))

            if conv_status:
                # The converter failed, keep track of this
                warning(f"{conv_name} failed on page {i}, file {file}")
                failed_pages.append(i)

    # Pass failed pages to pdflatex
    if len(failed_pages) > 0:
        warning("Now trying to obtain failed previews through pdflatex")
        error_count = legacy_conversion_pdflatex(latex_file, failed_pages,
            legacy_metrics, use_pdftocairo, conv, gs_device, gs_ext, alpha,
            resolution, output_format)
    else:
        error_count = 0

    # Invalidate metrics for pages that produced errors
    if len(error_pages) > 0:
        for index in error_pages:
            if index not in failed_pages:
                legacy_metrics.pop(index - 1)
                legacy_metrics.insert(index - 1, (index, -1.0))
                error_count += 1

    # Crop the ppm images
    if pnmcrop != None and output_format == "ppm":
        crop_files(pnmcrop, latex_file_root)

    # Allow to skip .metrics creation for custom management
    # (see the dvipng method)
    if not skipMetrics:
        # Extract metrics info from the log file.
        metrics_file = latex_file_re.sub(".metrics", latex_file)
        write_metrics_info(legacy_metrics, metrics_file)
        if error_count:
            warning("Failed to produce %d preview snippet(s)" % error_count)

    return (0, legacy_metrics)


if __name__ == "__main__":
    sys.exit(legacy_conversion(sys.argv)[0])
