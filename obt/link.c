/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   obt/link.c for the Openbox window manager
   Copyright (c) 2009        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "obt/link.h"
#include "obt/ddparse.h"
#include "obt/paths.h"
#include <glib.h>

struct _ObtLink {
    guint ref;

    ObtLinkType type;
    gchar *name; /*!< Specific name for the object (eg Firefox) */
    gboolean display; /*<! When false, do not display this link in menus or
                           launchers, etc */
    gboolean deleted; /*<! When true, the Link could exist but is deleted
                           for the current user */
    gchar *generic; /*!< Generic name for the object (eg Web Browser) */
    gchar *comment; /*!< Comment/description to display for the object */
    gchar *icon; /*!< Name/path for an icon for the object */
    guint env_required; /*!< The environments that must be present to use this
                          link. */
    guint env_restricted; /*!< The environments that must _not_ be present to
                            use this link. */

    union _ObtLinkData {
        struct _ObtLinkApp {
            gchar *exec; /*!< Executable to run for the app */
            gchar *wdir; /*!< Working dir to run the app in */
            gboolean term; /*!< Run the app in a terminal or not */
            ObtLinkAppOpen open;

            /* XXX gchar**? or something better, a mime struct.. maybe
               glib has something i can use. */
            gchar **mime; /*!< Mime types the app can open */

            ObtLinkAppStartup startup;
            gchar *startup_wmclass;
        } app;
        struct _ObtLinkLink {
            gchar *addr;
        } url;
        struct _ObtLinkDir {
        } dir;
    } d;
};

ObtLink* obt_link_from_ddfile(const gchar *ddname, GSList *paths,
                              ObtPaths *p)
{
    ObtLink *link;
    GHashTable *groups, *keys;
    ObtDDParseGroup *g;
    ObtDDParseValue *v, *type, *name, *target;

    groups = obt_ddparse_file(ddname, paths);
    if (!groups) return NULL;
    g = g_hash_table_lookup(groups, "Desktop Entry");
    if (!g) {
        g_hash_table_destroy(groups);
        return NULL;
    }

    keys = obt_ddparse_group_keys(g);

    /* check that required keys exist */

    if (!(type = g_hash_table_lookup(keys, "Type")))
    { g_hash_table_destroy(groups); return NULL; }
    if (!(name = g_hash_table_lookup(keys, "Name")))
    { g_hash_table_destroy(groups); return NULL; }

    if (type->value.enumerable == OBT_LINK_TYPE_APPLICATION) {
        if (!(target = g_hash_table_lookup(keys, "Exec")))
        { g_hash_table_destroy(groups); return NULL; }
    }
    else if (type->value.enumerable == OBT_LINK_TYPE_URL) {
        if (!(target = g_hash_table_lookup(keys, "URL")))
        { g_hash_table_destroy(groups); return NULL; }
    }
    else
        target = NULL;

    /* parse all the optional keys and build ObtLink (steal the strings) */
    link = g_slice_new0(ObtLink);
    link->ref = 1;
    link->type = type->value.enumerable;
    if (link->type == OBT_LINK_TYPE_APPLICATION)
        link->d.app.exec = target->value.string, target->value.string = NULL;
    else if (link->type == OBT_LINK_TYPE_URL)
        link->d.url.addr = target->value.string, target->value.string = NULL;
    link->display = TRUE;

    if ((v = g_hash_table_lookup(keys, "Hidden")))
        link->deleted = v->value.boolean;

    if ((v = g_hash_table_lookup(keys, "NoDisplay")))
        link->display = !v->value.boolean;

    if ((v = g_hash_table_lookup(keys, "GenericName")))
        link->generic = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "Comment")))
        link->comment = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "Icon")))
        link->icon = v->value.string, v->value.string = NULL;

    if ((v = g_hash_table_lookup(keys, "OnlyShowIn")))
        link->env_required = v->value.environments;
    else
        link->env_required = 0;

    if ((v = g_hash_table_lookup(keys, "NotShowIn")))
        link->env_restricted = v->value.environments;
    else
        link->env_restricted = 0;

    if (link->type == OBT_LINK_TYPE_APPLICATION) {
        if ((v = g_hash_table_lookup(keys, "TryExec"))) {
            /* XXX spawn a thread to check TryExec? */
            link->display = link->display &&
                obt_paths_try_exec(p, v->value.string);
        }

        if ((v = g_hash_table_lookup(keys, "Path"))) {
            /* steal the string */
            link->d.app.wdir = v->value.string;
            v->value.string = NULL;
        }

        if ((v = g_hash_table_lookup(keys, "Terminal")))
            link->d.app.term = v->value.boolean;

        if ((v = g_hash_table_lookup(keys, "StartupNotify")))
            link->d.app.startup = v->value.boolean ?
                OBT_LINK_APP_STARTUP_PROTOCOL_SUPPORT :
                OBT_LINK_APP_STARTUP_NO_SUPPORT;
        else
            link->d.app.startup = OBT_LINK_APP_STARTUP_LEGACY_SUPPORT;

        /* XXX parse link->d.app.exec to determine link->d.app.open */

        /* XXX there's more app specific stuff */
    }

    else if (link->type == OBT_LINK_TYPE_URL) {
        /* XXX there's URL specific stuff */
    }

    return link;
}

void obt_link_ref(ObtLink *dd)
{
    ++dd->ref;
}

void obt_link_unref(ObtLink *dd)
{
    if (--dd->ref < 1) {
        g_slice_free(ObtLink, dd);
    }
}