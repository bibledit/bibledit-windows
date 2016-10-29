/*
Copyright (©) 2003-2016 Teus Benschop.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <library/bibledit.h>
#include <config/libraries.h>
#include <filter/url.h>
#include <filter/string.h>
#ifdef HAVE_LIBPROC
#include <libproc.h>
#endif
#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif
#include <database/logs.h>
#ifdef HAVE_VISUALSTUDIO
#include <windows.h>
#endif


void sigint_handler (int s)
{
  (void) s;
  // When pressing Ctrl-C, the system outputs a "^C".
  // It is cleaner to write a new line after that.
  cout << endl;
  // Initiate server(s) shutdown.
  bibledit_stop_library ();
}


string backtrace_path ()
{
  string path = filter_url_create_root_path (filter_url_temp_dir (), "backtrace.txt");
  return path;
}


#ifdef HAVE_EXECINFO
// http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
// To add linker flag -rdynamic is essential.
void sigsegv_handler (int sig)
{
  (void) sig;

  // Information.
  cout << "Segmentation fault, writing backtrace to " << backtrace_path () << endl;

  void *array[20];
  size_t size;
  
  // Get void*'s for all entries on the stack
  size = backtrace (array, 20);

  // Write entries to file (to be logged next time bibledit starts).
  int fd = open (backtrace_path ().c_str (), O_WRONLY|O_CREAT, 0666);
  backtrace_symbols_fd (array, size, fd);
  close (fd);

  exit (1);
}
#endif


#ifdef HAVE_VISUALSTUDIO
void my_invalid_parameter_handler(const wchar_t* expression, const wchar_t* function, const wchar_t* file,	unsigned int line, uintptr_t pReserved) {
  wstring wexpression(expression);
  string sexpression(wexpression.begin(), wexpression.end());
  wstring wfunction(function);
  string sfunction(wfunction.begin(), wfunction.end());
  wstring wfile (file);
  string sfile(wfile.begin(), wfile.end());
  Database_Logs::log ("Invalid parameter detected in function " + sfunction + " in file " + sfile + " line " + convert_to_string ((size_t)line) + " expression " + sexpression + ".");
}
#endif


int main (int argc, char **argv)
{
  (void) argc;
  (void) argv;
  
  // Ctrl-C initiates a clean shutdown sequence, so there's no memory leak.
  signal (SIGINT, sigint_handler);
  
#ifdef HAVE_EXECINFO
  // Handler for logging segmentation fault.
  signal (SIGSEGV, sigsegv_handler);
#endif

#ifdef HAVE_VISUALSTUDIO
  // Set our own invalid parameter handler for on Windows.
  _set_invalid_parameter_handler(my_invalid_parameter_handler);
  // Disable the message box for assertions on Windows.
  _CrtSetReportMode(_CRT_ASSERT, 0);
#endif

  // Get the executable path and base the document root on it.
  string webroot;
#ifndef HAVE_VISUALSTUDIO
  {
    // The following works on Linux but not on Mac OS X:
    char *linkname = (char *) malloc (256);
    memset (linkname, 0, 256); // valgrind uninitialized value(s)
    ssize_t r = readlink ("/proc/self/exe", linkname, 256);
    (void) r;
    webroot = filter_url_dirname (linkname);
    free (linkname);
  }
#endif
#ifdef HAVE_LIBPROC
  {
    // The following works on Linux and on Mac OS X:
    int ret;
    pid_t pid;
    char pathbuf [2048];
    pid = getpid ();
    ret = proc_pidpath (pid, pathbuf, sizeof (pathbuf));
    if (ret > 0 ) {
      webroot = filter_url_dirname (pathbuf);
    }
  }
#endif
#ifdef HAVE_VISUALSTUDIO
  {
    // Getting the web root on Windows.
	// The following gets the path to the server.exe.
    // char buf[MAX_PATH] = { 0 };
    // DWORD ret = GetModuleFileNameA(NULL, buf, MAX_PATH);
	// While developing, the .exe runs in folder Debug or Release, and not in the expected folder.
	// Therefore it's better to take the path of the current directory.
    wchar_t buffer[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, buffer);
	char chars[MAX_PATH];
	char def_char = ' ';
	WideCharToMultiByte(CP_ACP, 0, buffer, -1, chars, MAX_PATH, &def_char, NULL);
	webroot = chars;
  }
#endif
  bibledit_initialize_library (webroot.c_str(), webroot.c_str());
  
  // Start the Bibledit library.
  bibledit_start_library ();
  bibledit_log ("The server started");
  cout << "Listening on http://localhost:" << config_logic_http_network_port ();
#ifndef HAVE_CLIENT
  cout << " and https://localhost:" << config_logic_https_network_port ();
#endif
  cout << endl;
  cout << "Press Ctrl-C to quit" << endl;

  // Log possible backtrace from a previous crash.
  string backtrace = filter_url_file_get_contents (backtrace_path ());
  filter_url_unlink (backtrace_path ());
  if (!backtrace.empty ()) {
    Database_Logs::log ("Backtrace of the last segmentation fault:");
    vector <string> lines = filter_string_explode (backtrace, '\n');
    for (auto & line : lines) {
      Database_Logs::log (line);
    }
  }

#ifndef HAVE_VISUALSTUDIO
  // Bibledit Cloud should restart itself at midnight.
  // This is to be sure that any memory leaks don't accumulate too much
  // in case Bibledit Cloud would run for months and years.
  // The Windows port uses this ./server also, but should not quit at midnight.
  bibledit_set_quit_at_midnight ();
#endif
  
  // Keep running till Bibledit stops or gets interrupted.
  while (bibledit_is_running ()) { }

  bibledit_shutdown_library ();

  return EXIT_SUCCESS;
}
