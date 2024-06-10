#! /usr/bin/python3

# file lyxpaperview.py
# This file is part of LyX, the document processor.
# Licence details can be found in the file COPYING.

# author Jürgen Spitzmüller
# This draws on a bash script and conceptual idea by Pavel Sanda
# Full author contact details are available in file CREDITS

# This script searches the home directory for a PDF or PS
# file with a name containing specific keywords (year and author by default).
# If found, it returns the path(s), separated by \n.

import os, sys, subprocess

def message(message):
    sys.stderr.write("lyxpaperview: %s\n" % message)

def error(message):
    sys.stderr.write("lyxpaperview error: %s\n" % message)
    exit(1)

def usage(prog_name):
    msg = "Usage: %s titletoken-1 [titletoken-2] ... [titletoken-n]\n" \
          "    Each title token must occur in the filename (at an arbitrary position).\n" \
          "    You might use quotes to enter multi-word tokens"
    return  msg % prog_name

# Copied from lyxpreview_tools.py
# PATH and PATHEXT environment variables
path = os.environ["PATH"].split(os.pathsep)
extlist = ['']
if "PATHEXT" in os.environ:
    extlist += os.environ["PATHEXT"].split(os.pathsep)
extlist.append('.py')

def find_exe(candidates):
    global extlist, path

    for command in candidates:
        prog = command.split()[0]
        for directory in path:
            for ext in extlist:
                full_path = os.path.join(directory, prog + ext)
                if os.access(full_path, os.X_OK):
                    # The thing is in the PATH already (or we wouldn't
                    # have found it). Return just the basename to avoid
                    # problems when the path to the executable contains
                    # spaces.
                    if full_path.lower().endswith('.py'):
                        return command.replace(prog, '"%s" "%s"'
                            % (sys.executable, full_path))
                    return command

    return None


def find(args, path):
    if os.name != 'nt':
        # use locate if possible (faster)
        if find_exe(['locate']):
            p1 = subprocess.Popen(['locate', '-i', args[0].lower()], stdout=subprocess.PIPE)
            px = subprocess.Popen(['grep', '-Ei', r'\.pdf$|\.ps$'], stdin=p1.stdout, stdout=subprocess.PIPE)
            for arg in args:
               if arg == args[0]:
                   # have this already
                   continue
               px = subprocess.Popen(['grep', '-i', arg], stdin=px.stdout, stdout=subprocess.PIPE)
            p1.stdout.close()
            output = px.communicate()
            return output[0].decode("utf8").strip('\n')
     # FIXME add something for windows as well?
     # Maybe dir /s /b %WINDIR%\*author* | findstr .*year.*\."ps pdf"

    for root, dirs, files in os.walk(path):
        for fname in files:
            lfname = fname.lower()
            if lfname.endswith(('.pdf', '.ps')):
                caught = True
                for arg in args:
                    if lfname.find(arg.lower()) == -1:
                        caught = False
                        break
                if caught:
                    return os.path.join(root, fname)
    return ""

def main(argv):
    progname = argv[0]
    
    args = sys.argv[1:]
    
    if len(args) < 1:
      error(usage(progname))

    result = find(args, path = os.environ["HOME"])
     
    print(result)
    exit(0)

if __name__ == "__main__":
    main(sys.argv)
