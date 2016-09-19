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


#include <paratext/logic.h>
#include <filter/url.h>
#include <filter/string.h>
#include <filter/roles.h>
#include <filter/usfm.h>
#include <filter/merge.h>
#include <filter/shell.h>
#ifndef HAVE_VISUALSTUDIO
#include <pwd.h>
#endif
#include <database/books.h>
#include <database/logs.h>
#include <database/config/bible.h>
#include <database/config/general.h>
#include <database/bibles.h>
#include <tasks/logic.h>
#include <locale/translate.h>
#include <bible/logic.h>


string Paratext_Logic::searchProjectsFolder ()
{
  const char *homedir;

  // Try Linux.
  if ((homedir = getenv("HOME")) == NULL) {
#ifndef HAVE_VISUALSTUDIO
    homedir = getpwuid(getuid())->pw_dir;
#endif
  }
  if (homedir) {
    vector <string> files = filter_url_scandir (homedir);
    for (auto file : files) {
      if (file.find ("Paratext") != string::npos) {
        return filter_url_create_path (homedir, file);
      }
    }
  }
  
#ifdef HAVE_VISUALSTUDIO
  // Try Windows.
  homedir = "C:\\";
  vector <string> files = filter_url_scandir (homedir);
  for (auto file : files) {
    if (file.find ("Paratext") != string::npos) {
      string path = filter_url_create_path (homedir, file);
      path = filter_string_str_replace ("\\\\", "\\", path);
      return path;
    }
  }
#endif

  // No Paratext projects folder found.
  return "";
}


vector <string> Paratext_Logic::searchProjects (string projects_folder)
{
  vector <string> projects;
  vector <string> folders = filter_url_scandir (projects_folder);
  for (auto folder : folders) {
    string path = filter_url_create_path (projects_folder, folder);
    if (filter_url_is_dir (path)) {
      map <int, string> books = searchBooks (path);
      if (!books.empty ()) projects.push_back (folder);
    }
  }
  return projects;
}


map <int, string> Paratext_Logic::searchBooks (string project_path)
{
  map <int, string> books;
  vector <string> files = filter_url_scandir (project_path);
  for (auto file : files) {
    if (file.find (".BAK") != string::npos) continue;
    if (file.find ("~") != string::npos) continue;
    string path = filter_url_create_path (project_path, file);
    int id = getBook (path);
    if (id) books [id] = file;
  }
  return books;
}


int Paratext_Logic::getBook (string filename)
{
  // A USFM file should not be larger than 4 Mb and not be smaller than 7 bytes.
  int filesize = filter_url_filesize (filename);
  if (filesize < 7) return 0;
  if (filesize > 4000000) return 0;
  
  // Read a small portion of the file for higher speed.
  ifstream fin (filename);
  fin.seekg (0);
  char buffer [128];
  fin.read (buffer, 7);
  buffer [7] = 0;
  string fragment (buffer);

  // Check for "\id "
  if (fragment.find ("\\id ") == string::npos) return 0;
  fragment.erase (0, 4);
  
  // Get book from the USFM id.
  int id = Database_Books::getIdFromUsfm (fragment);
  return id;
}


void Paratext_Logic::setup (string bible, string master)
{
  if (bible.empty ()) {
    Database_Logs::log ("No Bible given for Paratext link setup.");
    return;
  }
  if (master == "bibledit") {
    copyBibledit2Paratext (bible);
    Database_Config_Bible::setParatextCollaborationEnabled (bible, true);
  } else if (master == "paratext") {
    copyParatext2Bibledit (bible);
    Database_Config_Bible::setParatextCollaborationEnabled (bible, true);
  } else {
    Database_Logs::log ("Unknown master copy for Paratext link setup.");
  }
}


void Paratext_Logic::copyBibledit2Paratext (string bible)
{
  Database_Bibles database_bibles;
  
  Database_Logs::log ("Copying Bible from Bibledit to a Paratext project.");

  string project_folder = projectFolder (bible);

  Database_Logs::log ("Bibledit Bible: " + bible);
  Database_Logs::log ("Paratext project: " + project_folder);

  map <int, string> paratext_books = searchBooks (project_folder);
  
  vector <int> bibledit_books = database_bibles.getBooks (bible);
  for (int book : bibledit_books) {

    string bookname = Database_Books::getEnglishFromId (book);

    string paratext_book = paratext_books [book];

    string usfm;
    vector <int> chapters = database_bibles.getChapters (bible, book);
    for (int chapter : chapters) {
      if (!usfm.empty ()) usfm.append ("\n");
      usfm.append (database_bibles.getChapter (bible, book, chapter));
    }
    
    if (!paratext_book.empty ()) {

      string path = filter_url_create_path (projectFolder (bible), paratext_book);
      Database_Logs::log (bookname + ": " "Storing to:" " " + path);
      filter_url_file_put_contents (path, usfm);
      
      paratext_books [book].clear ();
    
    } else {

      Database_Logs::log (bookname + ": " "It could not be stored because the Paratext project does not have this book." " " "Create it, then retry.");
    
    }

    // Ancestor data needed for future merge.
    ancestor (bible, book, usfm);
  }
  
  for (auto element : paratext_books) {
    string paratext_book = element.second;
    if (paratext_book.empty ()) continue;
    Database_Logs::log (paratext_book + ": " "This Paratext project file was not affected.");
  }
}


void Paratext_Logic::copyParatext2Bibledit (string bible)
{
  Database_Bibles database_bibles;

  Database_Logs::log ("Copying Paratext project to a Bible in Bibledit.");
  
  string project_folder = projectFolder (bible);
  
  Database_Logs::log ("Paratext project: " + project_folder);
  Database_Logs::log ("Bibledit Bible: " + bible);

  vector <int> bibledit_books = database_bibles.getBooks (bible);

  map <int, string> paratext_books = searchBooks (project_folder);
  for (auto element : paratext_books) {

    int book = element.first;
    string bookname = Database_Books::getEnglishFromId (book);

    string paratext_book = element.second;
    string path = filter_url_create_path (projectFolder (bible), paratext_book);

    Database_Logs::log (bookname + ": " "Scheduling import from:" " " + path);

    // It is easiest to schedule an import task.
    // The task will take care of everything, including recording what to send to the Cloud.
    tasks_logic_queue (IMPORTBIBLE, { path, bible });

    // Ancestor data needed for future merge.
    string usfm = filter_url_file_get_contents (path);
    ancestor (bible, book, usfm);
  }
}


string Paratext_Logic::projectFolder (string bible)
{
  return filter_url_create_path (Database_Config_General::getParatextProjectsFolder (), Database_Config_Bible::getParatextProject (bible));
}


void Paratext_Logic::ancestor (string bible, int book, string usfm)
{
  string path = ancestorPath (bible, book);
  filter_url_file_put_contents (path, usfm);
}


string Paratext_Logic::ancestor (string bible, int book)
{
  string path = ancestorPath (bible, book);
  return filter_url_file_get_contents (path);
}


string Paratext_Logic::ancestorPath (string bible, int book)
{
  string path = filter_url_create_root_path ("paratext", "ancestors", bible);
  if (!file_or_dir_exists (path)) filter_url_mkdir (path);
  if (book) path = filter_url_create_path (path, convert_to_string (book));
  return path;
}


vector <string> Paratext_Logic::enabledBibles ()
{
  vector <string> enabled;
  Database_Bibles database_bibles;
  vector <string> bibles = database_bibles.getBibles ();
  for (auto bible : bibles) {
    if (Database_Config_Bible::getParatextCollaborationEnabled (bible)) {
      enabled.push_back (bible);
    }
  }
  return enabled;
}


void Paratext_Logic::synchronize ()
{
  // The Bibles for which Paratext synchronization has been enabled.
  vector <string> bibles = enabledBibles ();
  if (bibles.empty ()) return;

  
  Database_Bibles database_bibles;

  
  Database_Logs::log (synchronizeStartText (), Filter_Roles::translator ());
  

  /*
  When Bibledit writes changes to Paratext's USFM files, Paratext does not reload those changed USFM files. Thus Bibledit may overwrite changes made by others in the loaded chapter.
  Best would be that Bibledit refuses to sync while Paratext is opened. Bibledit would then give a message in its interface that changes for Paratext are ready, and that they will go through after closing Paratext. It keeps checking on Paratext while it syncs, and stops syncing right away when Paratext opens again. The library could contain the Linux check on Paratext running, and have an interface API for the Windows check. When Paratext is on, it refuses to sync, and gives a message in the log about that. On startup, the Windows exe immediately check whether Paratext runs to be sure the library does not start sync while Paratext is on.
  We might check what happens with Paratext Live, whether that gets updated immediately or not.
  Else only update the USFM files when Paratext does not run: This is a fool-proof method of update.
   
  On Linux the code below works well: It does not sync with Paratext while Paratext is running.
  On Windows 7 with Cygwin it works well too.
  On Windows XP without Cygwin, it fails to work at all.
  For this reason, Bibledit does not now check whether Paratext runs.
  The user may do this manually.

  bool paratext_running = false;
  vector <string> processes = filter_shell_active_processes ();
  for (auto p : processes) {
    if (p.find ("Paratext") != string::npos) paratext_running = true;
  }
  if (paratext_running) {
    Database_Logs::log ("Cannot synchronize while Paratext is running", Filter_Roles::translator ());
    return;
  }
  */
  
  
  // Go through each Bible.
  for (auto bible : bibles) {
  
    
    // The Paratext project folder for the current Bible.
    string project_folder = projectFolder (bible);
    if (!file_or_dir_exists (project_folder)) {
      Database_Logs::log ("Cannot find Paratext project folder:" " " + project_folder, Filter_Roles::translator ());
      continue;
    }

    
    string stylesheet = Database_Config_Bible::getExportStylesheet (bible);
    
    
    vector <int> bibledit_books = database_bibles.getBooks (bible);
    map <int, string> paratext_books = searchBooks (project_folder);

    
    for (auto book : bibledit_books) {

      
      // Check whether the book exists in the Paratext project, if not, skip it.
      string paratext_book = paratext_books [book];
      if (paratext_book.empty ()) {
        Database_Logs::log (journalTag (bible, book, -1) + "The Paratext project does not have this book", Filter_Roles::translator ());
        continue;
      }
      

      // Ancestor USFM per chapter.
      map <int, string> ancestor_usfm;
      {
        string usfm = ancestor (bible, book);
        vector <int> chapters = usfm_get_chapter_numbers (usfm);
        for (auto chapter : chapters) {
          string chapter_usfm = usfm_get_chapter_text (usfm, chapter);
          ancestor_usfm [chapter] = chapter_usfm;
        }
      }


      // Paratext USFM per chapter.
      map <int, string> paratext_usfm;
      {
        string path = filter_url_create_path (projectFolder (bible), paratext_book);
        string usfm = filter_url_file_get_contents (path);
        vector <int> chapters = usfm_get_chapter_numbers (usfm);
        for (auto chapter : chapters) {
          string chapter_usfm = usfm_get_chapter_text (usfm, chapter);
          paratext_usfm [chapter] = chapter_usfm;
        }
      }

      
      // Assemble the available chapters in this book
      // by combining the available chapters in the Bible in Bibledit
      // with the available chapters in the Paratext project.
      vector <int> chapters = database_bibles.getChapters (bible, book);
      for (auto element : paratext_usfm) {
        chapters.push_back (element.first);
      }
      chapters = array_unique (chapters);
      sort (chapters.begin(), chapters.end());
      

      bool book_is_updated = false;
      
      
      for (int chapter : chapters) {
        
        
        string usfm;
        string ancestor = ancestor_usfm [chapter];
        string bibledit = database_bibles.getChapter (bible, book, chapter);
        string paratext = paratext_usfm [chapter];
        
        
        if (!bibledit.empty () && paratext.empty ()) {
          // If Bibledit has the chapter, and Paratext does not, take the Bibledit chapter.
          usfm = bibledit;
          Database_Logs::log (journalTag (bible, book, chapter) + "Copy Bibledit to Paratext", Filter_Roles::translator ());
          ancestor_usfm [chapter] = usfm;
          paratext_usfm [chapter] = usfm;
        }
        else if (bibledit.empty () && !paratext.empty ()) {
          // If Paratext has the chapter, and Bibledit does not, take the Paratext chapter.
          usfm = paratext;
          Database_Logs::log (journalTag (bible, book, chapter) + "Copy Paratext to Bibledit", Filter_Roles::translator ());
          ancestor_usfm [chapter] = usfm;
          paratext_usfm [chapter] = usfm;
        }
        else if (filter_string_trim (bibledit) == filter_string_trim (paratext)) {
          // Bibledit and Paratext are the same: Do nothing.
        }
        else if (!ancestor.empty ()) {
          // If ancestor data exists, and Bibledit and Paratext differ,
          // merge both chapters, giving preference to Paratext,
          // as Paratext is more likely to contain the preferred version,
          // since it is assumed that perhaps a Manager runs Paratext,
          // and perhaps Translators run Bibledit.
          // But this assumption may be wrong.
          // Nevertheless preference must be given to some data anyway.
          vector <tuple <string, string, string, string, string>> conflicts;
          usfm = filter_merge_run (ancestor, bibledit, paratext, true, conflicts);
          Database_Logs::log (journalTag (bible, book, chapter) + "Chapter merged", Filter_Roles::translator ());
          ancestor_usfm [chapter] = usfm;
          paratext_usfm [chapter] = usfm;
          // Log the change.
          bible_logic_log_change (bible, book, chapter, usfm, "", "Paratext", true);
        } else {
          Database_Logs::log (journalTag (bible, book, chapter) + "Cannot merge chapter due to missing parent data", Filter_Roles::translator ());
        }
      
        
        if (!usfm.empty ()) {
          // Store the updated chapter in Bibledit.
          bible_logic_store_chapter (bible, book, chapter, usfm);
          book_is_updated = true;
        }

      }

      
      // The whole book has now been dealt with here at this point.
      // The Bibledit data is already up to date or has been updated.
      // In case of any updates made in this book, update the ancestor data and the Paratext data.
      if (book_is_updated) {
        
        string usfm;
        for (auto element : ancestor_usfm) {
          string data = element.second;
          if (!data.empty ()) {
            if (!usfm.empty ()) usfm.append ("\n");
            usfm.append (filter_string_trim (data));
          }
        }
        ancestor (bible, book, usfm);
        
        usfm.clear ();
        for (auto element : paratext_usfm) {
          string data = element.second;
          if (!data.empty ()) {
            if (!usfm.empty ()) usfm.append ("\n");
            usfm.append (filter_string_trim (data));
          }
        }
        string path = filter_url_create_path (projectFolder (bible), paratext_book);
        filter_url_file_put_contents (path, usfm);
      }
      

    }
    
    
  }
  
  
  Database_Logs::log (synchronizeReadyText (), Filter_Roles::translator ());
}


string Paratext_Logic::synchronizeStartText ()
{
  return translate ("Paratext: Send/receive");
}


string Paratext_Logic::synchronizeReadyText ()
{
  return translate ("Paratext: Up to date");
}


// Create tag for the journal.
// If chapter is negative, it is left out from the tag.
string Paratext_Logic::journalTag (string bible, int book, int chapter)
{
  string bookname = Database_Books::getEnglishFromId (book);
  string project = Database_Config_Bible::getParatextProject (bible);
  string fragment = bible + " <> " + project + " " + bookname;
  if (chapter >= 0) fragment.append (" " + convert_to_string (chapter));
  fragment.append (": ");
  return fragment;
}
