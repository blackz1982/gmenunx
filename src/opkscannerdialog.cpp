/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "opkscannerdialog.h"
#include "utilities.h"
#include "menu.h"
#include "powermanager.h"
#include "debug.h"
#include "linkapp.h"
#include <dirent.h>
#include <libopk.h>

using namespace std;
extern const char *CARD_ROOT;

OPKScannerDialog::OPKScannerDialog(GMenu2X *gmenu2x, const string &title, const string &description, const string &icon, const string &backdrop)
	: TextDialog(gmenu2x, title, description, icon, backdrop)
{}

void OPKScannerDialog::opkInstall(const string &path) {
	string pkgname = base_name(path, true);

	struct OPK *opk = opk_open(path.c_str());

	if (!opk) {
		text.push_back(path + ": " + gmenu2x->tr["Unable to open OPK"]);
		lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

		ERROR("%s: Unable to open OPK", path.c_str());
		return;
	}

	while (true) {
		const char *name;
		int ret = opk_open_metadata(opk, &name);
		if (ret < 0) {
			text.push_back(path + ": " + gmenu2x->tr["Error loading meta-data"]);
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
			ERROR("Error loading meta-data");
			goto close;
		} else if (ret == 0) {
			goto close;
		}

		/* Strip .desktop */
		string linkname(name), platform;
		string::size_type pos = linkname.rfind('.');
		linkname = linkname.substr(0, pos);

		pos = linkname.rfind('.');
		platform = linkname.substr(pos + 1);

		linkname = pkgname + "." + linkname + ".lnk";

		if (!(platform == PLATFORM || platform == "all")) {
			text.push_back(pkgname + ": " + gmenu2x->tr["Unsupported platform"] + " '" + platform + "'");
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

			ERROR("%s: Unsupported platform '%s'", pkgname.c_str(), platform.c_str());
#if !defined(TARGET_PC) // pc debug
			continue;
#endif
		}

		const char *key, *val;
		size_t lkey, lval;

		string
			title = "",
			params = "",
			description = "",
			manual = "",
			selectordir = "",
			selectorfilter = "",
			aliasfile = "",
			icon = "",
			section = "applications";

		while (ret = opk_read_pair(opk, &key, &lkey, &val, &lval)) {
			if (ret < 0) {
				text.push_back(path + ": " + gmenu2x->tr["Error loading meta-data"]);
				lineWidth = drawText(&text, firstCol, -1, rowsPerPage);
				ERROR("Error loading meta-data");
				goto close;
			} else if (ret == 0) {
				goto close;
			}

			char buf[lval + 1];
			sprintf(buf, "%.*s", lval, val);

			if (!strncmp(key, "Name", lkey)) {
				title = buf;
			} else
			if (!strncmp(key, "Exec", lkey)) {
				params = buf;
			} else
			if (!strncmp(key, "Comment", lkey)) {
				description = buf;
			} else
			if (!strncmp(key, "X-OD-Manual", lkey)) {
				manual = buf;
			} else
			if (!strncmp(key, "X-OD-Selector", lkey)) {
				selectordir = buf;
			} else
			if (!strncmp(key, "X-OD-Filter", lkey)) {
				selectorfilter = buf;
			} else
			if (!strncmp(key, "X-OD-Alias", lkey)) {
				aliasfile = buf;
			} else
			if (!strncmp(key, "Categories", lkey)) {
				section = buf;
				pos = section.find(';');
				if (pos != section.npos) {
					section = section.substr(0, pos);
				}
			} else
			if (!strncmp(key, "Icon", lkey)) {
				icon = (string)buf + ".png";
			}
		}

		gmenu2x->menu->addSection(section);

		string linkpath = "sections/" + section + "/" + linkname;

		LinkApp *link = new LinkApp(gmenu2x, gmenu2x->input, linkpath.c_str());

		if (!path.empty()) link->setExec(path);
		if (!params.empty()) link->setParams(path);
		if (!title.empty()) link->setTitle(title);
		if (!description.empty()) link->setDescription(description);
		if (!manual.empty()) link->setManual(manual);
		if (!selectordir.empty()) link->setSelectorDir(selectordir);
		if (!selectorfilter.empty()) link->setSelectorFilter(selectorfilter);
		if (!aliasfile.empty()) link->setAliasFile(aliasfile);
		if (!icon.empty()) link->setIcon(icon);

		link->save();
	}

	close:
	opk_close(opk);
}

void OPKScannerDialog::opkScan(string opkdir) {
	DIR *dirp;
	struct dirent *dptr;

	opkdir = real_path(opkdir);

	if ((dirp = opendir(opkdir.c_str())) == NULL) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_type != DT_REG || dptr->d_name[0] == '.')
			continue;

		string filepath = opkdir + "/" + dptr->d_name;
		if (file_ext(filepath, true) == ".opk") {

			text.push_back(gmenu2x->tr["Installing"] + " " + (string)dptr->d_name);
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

			opkInstall(filepath);
		}
	}
	closedir(dirp);
}

void OPKScannerDialog::exec() {
	bool inputAction = false;
	rowsPerPage = gmenu2x->listRect.h/gmenu2x->font->getHeight();

	gmenu2x->powerManager->clearTimer();

	if (gmenu2x->sc.skinRes(this->icon) == NULL)
		this->icon = "icons/terminal.png";

	buttons.push_back({"skin:imgs/manual.png", gmenu2x->tr["Running.. Please wait.."]});

	drawDialog(this->bg);

	gmenu2x->s->flip();

	DIR *dirp0, *dirp1;
	struct dirent *dptr0, *dptr1;

	string subdir(CARD_ROOT);

	// scan HOME
	if ((dirp1 = opendir(subdir.c_str())) != NULL)
	while ((dptr1 = readdir(dirp1))) {
		if (dptr1->d_type != DT_DIR || dptr1->d_name[0] == '.')
			continue;

		string dname = subdir + "/" + dptr1->d_name;

		text.push_back(gmenu2x->tr["Scanning"] + " " + dname);
		lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

		opkScan(dname);
	}
	closedir(dirp1);

	// scan MEDIA
	string opkdir = "/media";

	if ((dirp0 = opendir(opkdir.c_str())) != NULL)
	while ((dptr0 = readdir(dirp0))) {
		if (dptr0->d_type != DT_DIR || dptr0->d_name[0] == '.')
			continue;

		subdir = opkdir + "/" + dptr0->d_name;

		if ((dirp1 = opendir(subdir.c_str())) != NULL)
		while ((dptr1 = readdir(dirp1))) {
			if (dptr1->d_type != DT_DIR || dptr1->d_name[0] == '.')
				continue;

			string dname = subdir + "/" + dptr1->d_name;

			text.push_back(gmenu2x->tr["Scanning"] + " " + dname);
			lineWidth = drawText(&text, firstCol, -1, rowsPerPage);

			opkScan(dname);
		}
		closedir(dirp1);
	}
	closedir(dirp0);

	system("sync &");

	text.push_back("----");
	text.push_back(gmenu2x->tr["Done"]);
	if (text.size() >= rowsPerPage) firstRow = text.size() - rowsPerPage;

	buttons.clear();
	buttons.push_back({"dpad", gmenu2x->tr["Scroll"]});
	buttons.push_back({"b", gmenu2x->tr["Exit"]});

	gmenu2x->bg->blit(this->bg,0,0);
	drawDialog(this->bg);
	gmenu2x->s->flip();

	while (true) {
		lineWidth = drawText(&text, firstCol, firstRow, rowsPerPage);

		inputAction = gmenu2x->input.update();

		if (gmenu2x->input[UP] && firstRow > 0) firstRow--;
		else if (gmenu2x->input[DOWN] && firstRow + rowsPerPage < text.size()) firstRow++;
		else if (gmenu2x->input[RIGHT] && firstCol > -1 * (lineWidth - gmenu2x->listRect.w) - 10) firstCol -= 30;
		else if (gmenu2x->input[LEFT]  && firstCol < 0) firstCol += 30;
		else if (gmenu2x->input[PAGEUP] || (gmenu2x->input[LEFT] && firstCol == 0)) {
			if (firstRow >= rowsPerPage - 1)
				firstRow -= rowsPerPage - 1;
			else
				firstRow = 0;
		}
		else if (gmenu2x->input[PAGEDOWN] || (gmenu2x->input[RIGHT] && firstCol == 0)) {
			if (firstRow + rowsPerPage * 2 - 1 < text.size())
				firstRow += rowsPerPage - 1;
			else
				firstRow = max(0,text.size()-rowsPerPage);
		}
		else if (gmenu2x->input[SETTINGS] || gmenu2x->input[CANCEL]) {
			gmenu2x->powerManager->resetSuspendTimer();
			return;
		}
	}
}
