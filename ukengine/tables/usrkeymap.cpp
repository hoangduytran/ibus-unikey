// -*- mode:c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-
/* Unikey Vietnamese Input Method
 * Copyright (C) 2000-2005 Pham Kim Long
 * Contact:
 *   unikey@gmail.com
 *   UniKey project: http://unikey.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <iostream>
#include <string.h>
#include <stdio.h>
using namespace std;

#include <ctype.h>
#include "usrkeymap.h"


// Forward declaration: resolve an action/event code to a human-readable label index.
int getLabelIndex(int action);
// Forward declaration: initialize all key slots to "no special action".
void initKeyMap(int keyMap[256]);

// Comment prefix in user keymap files. Everything after this char is ignored.
#define OPT_COMMENT_CHAR ';'

// Pair a textual command label from config files with an internal UniKey event id.
struct UkEventLabelPair
{
    char label[32];
    int ev;
};

// Canonical mapping table used by both parser (label->event) and serializer (event->label).
UkEventLabelPair UkEvLabelList[] = {
    {"Tone0", vneTone0},
    {"Tone1", vneTone1},
    {"Tone2", vneTone2},
    {"Tone3", vneTone3},
    {"Tone4", vneTone4},
    {"Tone5", vneTone5},
    {"Roof-All", vneRoofAll},
    {"Roof-A", vneRoof_a},
    {"Roof-E", vneRoof_e}, 
    {"Roof-O", vneRoof_o},
    {"Hook-Bowl", vneHookAll},
    {"Hook-UO", vneHook_uo},
    {"Hook-U", vneHook_u},
    {"Hook-O", vneHook_o},
    {"Bowl", vneBowl},
    {"D-Mark", vneDd},
    {"Telex-W", vne_telex_w},
    {"Escape", vneEscChar},
    {"DD", vneCount + vnl_DD},
    {"dd", vneCount + vnl_dd},
    {"A^", vneCount + vnl_Ar},
    {"a^", vneCount + vnl_ar},
    {"A(", vneCount + vnl_Ab},
    {"a(", vneCount + vnl_ab},
    {"E^", vneCount + vnl_Er},
    {"e^", vneCount + vnl_er},
    {"O^", vneCount + vnl_Or},
    {"o^", vneCount + vnl_or},
    {"O+", vneCount + vnl_Oh},
    {"o+", vneCount + vnl_oh},
    {"U+", vneCount + vnl_Uh},
    {"u+", vneCount + vnl_uh}
};

// Number of entries in the label<->event lookup table.
const int UkEvLabelCount = sizeof(UkEvLabelList)/sizeof(UkEventLabelPair);

//--------------------------------------------------
// Parse one "name = value" config line.
// - Removes inline comment part.
// - Trims spaces around name and value.
// - Returns 1 on successful parse, 0 for empty/invalid lines.
static int parseNameValue(char *line, char **name, char **value)
{
    char *p, *mark;
    char ch;

    if (line == 0)
        return 0;

    // get rid of comment
    p = strchr(line, OPT_COMMENT_CHAR);
    if (p)
        *p = 0;

    //get option name
    for (p=line; *p == ' '; p++);
    if (*p == 0)
        return 0;

    *name = p;
    mark = p; //mark the last non-space character
    p++;
    while ((ch=*p) != '=' && ch!=0) {
        if (ch != ' ')
            mark = p;
        p++;
    }

    if (ch == 0)
        return 0;
    *(mark+1) = 0; //terminate name with a null character

    //get option value
    p++;
    while (*p == ' ') p++;
    if (*p == 0)
        return 0;

    *value = p;
    mark = p;
    while (*p) { //strip trailing spaces
        if (*p != ' ')
            mark = p;
        p++;
    }
    *++mark = 0;
    return 1;
}

//-----------------------------------------------------
// Load a user layout file directly into keyMap[256].
// Progression:
// 1) Parse file into ordered key/action pairs.
// 2) Start from a default "all normal" map.
// 3) Apply each mapping; for tone actions, mirror lowercase key to same action.
DllExport int UkLoadKeyMap(const char *fileName, int keyMap[256])
{
    int i, mapCount;
    UkKeyMapPair orderMap[256];
    if (!UkLoadKeyOrderMap(fileName, orderMap, &mapCount))
        return 0;

    initKeyMap(keyMap);
    for (i=0; i < mapCount; i++) {
        keyMap[orderMap[i].key] = orderMap[i].action;
        if (orderMap[i].action < vneCount) {
            keyMap[tolower(orderMap[i].key)] = orderMap[i].action;
        }
    }
    return 1;
}

/**
* @brief Parse user key layout file into an ordered list of explicit mappings.
* @param fileName path to the key layout file to load
* @param pMap array of UkKeyMapPair entries to populate
* @param pMapCount input/output pointer to the size of pMap; receives loaded entry count
* @return status code indicating success or failure
*/
DllExport int UkLoadKeyOrderMap(const char *fileName, UkKeyMapPair *pMap, int *pMapCount)
{
    FILE *f; // File pointer for key layout file
    char *buf;          // Reused line buffer for file input
    char *name, *value; // Parsed left/right tokens from a config line.
    size_t len; // Length of the current line
    int i, bufSize, lineCount; // Index for loop, buffer size, line count
    unsigned char c;    // Parsed key byte from "name".
    int mapCount;       // Number of accepted mappings written to pMap.
    int keyMap[256];    // Temporary occupancy map to reject duplicate assignments.

    f = fopen(fileName, "r"); // Open key layout file for reading
    if (f == 0) { // if file cannot be opened, return 0
        cerr << "Failed to open file: " << fileName << endl; // Print error message if file cannot be opened
        return 0; // Return 0 on failure
    }

    initKeyMap(keyMap); // Initialize key map with default "normal character" action for every byte value
    bufSize = 256; // Set buffer size to 256
    buf = new char[bufSize]; // Allocate buffer for key layout file

    lineCount = 0; // Set line count to 0
    mapCount = 0; // Set map count to 0
    while (!feof(f)) { // Loop through key layout file
        if (fgets((char *)buf, bufSize, f) == 0)
            break; // If end of file is reached, break
        lineCount++;
        len = strlen(buf); // Get length of current line
        if (len == 0)
            break; // If line is empty, break

        if (buf[len-1] == '\n') // If last character is a newline, set it to 0
            buf[len-1] = 0;
        if (parseNameValue(buf, (char **)&name, (char **)&value)) { // Parse name and value from current line
            if (strlen(name) == 1) { // If name is a single character, set c to the key
                // Loop through all labels and check if the label matches the value
                for (i=0; i < UkEvLabelCount; i++) { // Loop through all labels
                    if (strcmp(UkEvLabelList[i].label, value) == 0) { // If label matches value, set c to the key
                        c = (unsigned char)name[0]; // Set c to the key
                        if (keyMap[c] != vneNormal) { // If key is already assigned, don't accept this map
                            break; // Break out of loop
                        }
                }
                // If no label matches value, print error message
                if (i == UkEvLabelCount) { // If no label matches value, print error message
                    cerr << "Error in user key layout, line " << lineCount << ": command not found" << endl;
                }
            }
            else { // If key name is not a single character, print error message
                cerr << "Error in user key layout, line " << lineCount 
                     << ": key name is not a single character" << endl;	
            }
        }
    }
    delete [] buf; // Delete buffer
    fclose(f); // Close file

    *pMapCount = mapCount; // Set map count to the number of mappings   

    return 1; // Return 1 on success
}

/**
* @brief Fill key map with default "normal character" action for every byte value.
* @param keyMap array of 256 mapping values to fill
* @return void
*/
void initKeyMap(int keyMap[256])
{
    unsigned int c;
    for (c=0; c<256; c++)  // Loop through all key values
        keyMap[c] = vneNormal;  // Set all key values to vneNormal
} 

// Header emitted at top of saved user keymap files.
const char *UkKeyMapHeader = 
    "; This is UniKey user-defined key mapping file, generated from UniKey (Windows)\n\n";

/**
* @brief Persist an ordered key/action list to disk using "key = label" lines. Only actions with known labels are written.
* @param fileName path to the key order map file to write
* @param pMap array of UkKeyMapPair entries to write
* @param mapCount number of entries in pMap to store
* @return status code indicating success or failure
*/
DllExport int UkStoreKeyOrderMap(const char *fileName, UkKeyMapPair *pMap, int mapCount)
{
    FILE *f; // File pointer for key order map file
    int i; // Index for loop
    int labelIndex; // Index into UkEvLabelList for current action.
    char line[128]; // Output buffer for one serialized mapping line.

    f = fopen(fileName, "wt");  // Open file for writing in text mode
    if (f == 0) {       // if file cannot be opened, return 0
        cerr << "Failed to open file: " << fileName << endl;  // Print error message if file cannot be opened
        return 0; // Return 0 on failure
    }

    // Write header to file
    fputs(UkKeyMapHeader, f);  // Write header to file
    // Loop through all mappings and write them to file
    for (i=0; i < mapCount; i++) {  // Loop through all mappings
        // Get label index for current action
        labelIndex = getLabelIndex(pMap[i].action);  
        // If label index is not -1, write mapping to file
        if (labelIndex != -1) {  
            sprintf(line, "%c = %s\n", pMap[i].key, UkEvLabelList[labelIndex].label);  // Format mapping as "key = label" and write to file
            fputs(line, f);  // Write mapping to file
        }
    }
    fclose(f);  // Close file
    return 1;  // Return 1 on success 
}

/**
* @brief Find label table index by event code; returns -1 when no label exists.
* @param event event code to find
* @return index of label if found, -1 if not found
*/
int getLabelIndex(int event)
{
    int i;
    // Loop through all labels and check if the label matches the event
    for (i = 0; i < UkEvLabelCount; i++) {  
        // If label matches event, return index of label
        if (UkEvLabelList[i].ev == event)  
            return i;  
    }
    return -1;  
}
