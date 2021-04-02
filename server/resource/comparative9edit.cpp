/*
 Copyright (©) 2003-2021 Teus Benschop.
 
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


#include <resource/comparative9edit.h>
#include <assets/view.h>
#include <assets/page.h>
#include <assets/header.h>
#include <filter/roles.h>
#include <filter/string.h>
#include <filter/url.h>
#include <locale/translate.h>
#include <resource/logic.h>
#include <menu/logic.h>
#include <dialog/entry.h>
#include <dialog/yes.h>
#include <database/config/general.h>
#include <pugixml/pugixml.hpp>
#include <resource/comparative1edit.h>


using namespace pugi;


string resource_comparative9edit_url ()
{
  return "resource/comparative9edit";
}


bool resource_comparative9edit_acl (void * webserver_request)
{
  return Filter_Roles::access_control (webserver_request, Filter_Roles::manager ());
}


string resource_comparative9edit (void * webserver_request)
{
  Webserver_Request * request = (Webserver_Request *) webserver_request;

  
  string page;
  Assets_Header header = Assets_Header (translate("Comparative resources"), request);
  header.addBreadCrumb (menu_logic_settings_menu (), menu_logic_settings_text ());
  page = header.run ();
  Assets_View view;
  string error, success;
  

  // New comparative resource handler.
  if (request->query.count ("new")) {
    Dialog_Entry dialog_entry = Dialog_Entry ("comparative9edit", translate("Please enter a name for the new comparative resource"), "", "new", "");
    page += dialog_entry.run ();
    return page;
  }
  if (request->post.count ("new")) {
    // The title for the new resource as entered by the user.
    // Clean the title up and ensure it always starts with "Comparative ".
    // This word flags the comparative resource as being one of that category.
    string new_resource = request->post ["entry"];
    size_t pos = new_resource.find (resource_logic_comparative_resource_v2 ());
    if (pos != string::npos) {
      new_resource.erase (pos, resource_logic_comparative_resource_v2 ().length());
    }
    new_resource.insert (0, resource_logic_comparative_resource_v2 ());
    vector <string> titles;
    vector <string> resources = Database_Config_General::getComparativeResources ();
    for (auto resource : resources) {
      string title;
      if (resource_logic_parse_comparative_resource_v2 (resource, &title)) {
        titles.push_back (title);
      }
    }
    if (in_array (new_resource, titles)) {
      error = translate("This comparative resource already exists");
    } else if (new_resource.empty ()) {
      error = translate("Please give a name for the comparative resource");
    } else {
      // Store the new resource in the list.
      string resource = resource_logic_assemble_comparative_resource_v2 (new_resource);
      resources.push_back (resource);
      Database_Config_General::setComparativeResources (resources);
      success = translate("The comparative resource was created");
      // Redirect the user to the place where to edit that new resource.
      string url = resource_comparative1edit_url () + "?name=" + new_resource;
      redirect_browser (webserver_request, url);
      return "";
    }
  }

  
  // Delete resource.
  string title2remove = request->query ["delete"];
  if (!title2remove.empty()) {
    string confirm = request->query ["confirm"];
    if (confirm == "") {
      Dialog_Yes dialog_yes = Dialog_Yes ("comparative9edit", translate("Would you like to delete this resource?"));
      dialog_yes.add_query ("delete", title2remove);
      page += dialog_yes.run ();
      return page;
    } if (confirm == "yes") {
      vector <string> updated_resources;
      vector <string> existing_resources = Database_Config_General::getComparativeResources ();
      for (auto resource : existing_resources) {
        string title;
        resource_logic_parse_comparative_resource_v2 (resource, &title);
        if (title != title2remove) updated_resources.push_back (resource);
      }
      Database_Config_General::setComparativeResources (updated_resources);
      success = translate ("The resource was deleted");
    }
  }


  vector <string> resources = Database_Config_General::getComparativeResources ();
  string resourceblock;
  {
    xml_document document;
    for (auto & resource : resources) {
      string title;
      if (!resource_logic_parse_comparative_resource_v2 (resource, &title)) continue;
      xml_node p_node = document.append_child ("p");
      xml_node a_node = p_node.append_child("a");
      string href = "comparative1edit?name=" + title;
      a_node.append_attribute ("href") = href.c_str();
      title.append (" [" + translate("edit") + "]");
      a_node.text().set (title.c_str());
    }
    stringstream output;
    document.print (output, "", format_raw);
    resourceblock = output.str ();
  }
  view.set_variable ("resourceblock", resourceblock);

   
  view.set_variable ("success", success);
  view.set_variable ("error", error);
  page += view.render ("resource", "comparative9edit");
  page += Assets_Page::footer ();
  return page;
}
