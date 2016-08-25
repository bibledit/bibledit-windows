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


#include <timer/index.h>
#include <database/logs.h>
#include <database/config/general.h>
#include <database/state.h>
#include <config/globals.h>
#include <filter/string.h>
#include <filter/date.h>
#include <tasks/logic.h>
#include <tasks/run.h>
#include <sendreceive/logic.h>
#include <client/logic.h>
#include <changes/logic.h>
#include <checks/logic.h>
#include <export/logic.h>
#include <debug/logic.h>
#include <setup/logic.h>


// CPU-intensive actions run at night.
// This keeps the site more responsive during the day.


// The order for running the following nightly scripts is important.
// Any of those scripts may influence the subsequent ones.
// The order is such that all information generated is as recent as possible.
// More important tasks are done first, and the less important ones at the end.
// This leads to an order as visible in the code below.


void timer_index ()
{
  bool client = client_logic_client_enabled ();
  int previous_second = -1;
  int previous_minute = -1;
  int previous_fraction = -1;
  while (config_globals_http_running && config_globals_https_running) {

    try {

      // Wait shortly.
      this_thread::sleep_for (chrono::milliseconds (100));
      
      // Wait till the data structures have been initialized.
      setup_wait_till_main_folders_present ();
      if (!config_globals_data_initialized) continue;
      
      // The current time, localized.
      int seconds_since_epoch = filter_date_seconds_since_epoch ();
      int local_seconds = filter_date_local_seconds (seconds_since_epoch);
      int second = filter_date_numerical_second (local_seconds);
      int minute = filter_date_numerical_minute (local_seconds);
      int hour = filter_date_numerical_hour (local_seconds);
      int weekday = filter_date_numerical_week_day (local_seconds);
      
      // Run once per second.
      if (second == previous_second) continue;
      previous_second = second;

      // Every second: Deal with queued and/or active tasks.
      tasks_run_check ();
      
      // Run the part below every so many seconds.
      int fraction = second / 5;
      if (fraction != previous_fraction) {
        previous_fraction = fraction;
      }
      
      // Run the part below once per minute.
      if (minute == previous_minute) continue;
      previous_minute = minute;
      
      // Every minute send out queued email.
      tasks_logic_queue (SENDEMAIL);

      // Check for new mail every five minutes.
      // Do not check more often with gmail else the account may be shut down.
      if ((!client) && ((minute % 5) == 0)) {
        tasks_logic_queue (RECEIVEEMAIL);
      }

      // At the nineth minute after every full hour rotate the journal.
      // The nine is chosen, because the journal rotation will summarize the send/receive messages.
      // In case send/receive happens every five minutes, it is expected that under normal circumstances
      // the whole process of sending/receivning will be over, so summarization can then be done.
      if (minute == 9) tasks_logic_queue (ROTATEJOURNAL);
      
      // Client sends/receives Bibles and Consultation.
      sendreceive_queue_sync (minute);
      
      // Sending and receiving Bibles to and from the git repository.
      // On a production website running on an inexpensive virtual private server
      // with 512 Mbyte of memory and a fast network connection,
      // sending and receiving two Bibles takes more than 15 minutes when there are many changes.
      bool sendreceive = ((hour == 0) && (minute == 5));
      bool repeat = ((minute % 5) == 0);
      if (sendreceive || repeat) {
        sendreceive_queue_all (sendreceive);
      }
      
      // Deal with the changes in the Bible made per user.
      // Deal with notifications for the daily changes in the Bibles.
      // This takes a few minutes on a production machine with two Bibles and changes in several chapters.
      // It runs in a server configuration, not on a client.
      if (!client) {
        if ((hour == 0) && (minute == 20)) {
          changes_logic_start ();
        }
      }

      // Run the checks on the Bibles.
      // This takes 15 minutes on a production machine with two Bibles.
      if (!client) {
        if ((hour == 0) && (minute == 30)) {
          checks_logic_start_all ();
        }
      }
      
      // Database maintenance and trimming.
      // It takes a few minutes on a production machine.
      if ((hour == 0) && (minute == 50)) {
        tasks_logic_queue (MAINTAINDATABASE);
      }
      
      // Export the Bibles to the various output formats.
      // This may take an hour on a production machine.
      // This hour was in PHP. In C++ it is much faster.
      if (!client) {
        if ((hour == 1) && (minute == 10)) {
          Export_Logic::scheduleAll ();
        }
      }
      
      // Delete temporal files older than a few days.
      if ((hour == 2) && (minute == 0)) {
        tasks_logic_queue (CLEANTMPFILES);
      }
      
      // Software update notifications.
      if (!client && hour == 2 && minute == 20) {
        tasks_logic_queue (NOTIFYSOFTWAREUPDATES);
      }
      
      // Re-index Bibles and notes.
      // Only update missing indexes.
      if ((hour == 2) && (minute == 0)) {
        Database_State::create ();
        Database_Config_General::setIndexBibles (true);
        tasks_logic_queue (REINDEXBIBLES);
        Database_Config_General::setIndexNotes (true);
        tasks_logic_queue (REINDEXNOTES);
      }
      
      // Actions for a demo installation.
      if (minute == 10) {
        if (config_logic_demo_enabled ()) {
          tasks_logic_queue (CLEANDEMO);
        }
      }
      
      // Sprint burndown.
      // It runs every hour in the Cloud.
      // The script itself determines what to do at which hour of the day or day of the week or day of the month.
      if (!client) {
        if (minute == 5) {
          tasks_logic_queue (SPRINTBURNDOWN);
        }
      }

      // Quit at midnight if flag is set.
      if (config_globals_quit_at_midnight) {
        if (hour == 0) {
          if (minute == 1) {
            if (!Database_Config_General::getJustStarted ()) {
              if (tasks_run_active_count ()) {
                Database_Logs::log ("Server is due to restart itself but does not because of active jobs");
              } else {
                Database_Logs::log ("Server restarts itself");
                exit (0);
              }
            }
          }
          // Clear flag in preparation of restart next minute.
          // This flag also has the purpose of ensuring the server restarts once during that minute,
          // rather than restarting repeatedly many times during that minute.
          if (minute == 0) {
            Database_Config_General::setJustStarted (false);
          }
        }
      }
      
      // Email notes statistics to the users.
      if (!client) {
        if ((hour == 3) && (minute == 0)) {
          tasks_logic_queue (NOTESSTATISTICS);
        }
      }

      // Update SWORD stuff once a week.
      if (weekday == 1) {
        // Refresh module list.
        if ((!client) && (hour == 3) && (minute == 5)) {
          tasks_logic_queue (REFRESHSWORDMODULES);
        }
        // Update installed SWORD modules, shortly after the module list has been refreshed.
        if ((!client) && (hour == 3) && (minute == 15)) {
          tasks_logic_queue (UPDATESWORDMODULES);
        }
      }
      
      // The Cloud updates the list of USFM resources once a week.
      if (weekday == 1) {
        if ((!client) && (hour == 3) && (minute == 10)) {
          tasks_logic_queue (LISTUSFMRESOURCES);
        }
      }
      

    } catch (exception & e) {
      Database_Logs::log (e.what ());
    } catch (exception * e) {
      Database_Logs::log (e->what ());
    } catch (...) {
      Database_Logs::log ("A general internal error occurred in the timers");
    }

  }
}
