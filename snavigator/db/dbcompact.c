/*
 Copyright (c) 2010, Freek

 This file is part of Source-Navigator.

 Source-Navigator is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2, or (at your option)
 any later version.

 Source-Navigator is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with Source-Navigator; see the file COPYING.  If not, write to
 the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
 MA 02111-1307, USA.
*/

/*
 dbcompact will try to optimize a db file via a call to DB->compact()
 According to the docs we need to call compact() twice to fully
 exploit the tree/page reorganization.

 line protocol for communicating with SN
 Status: Optimizing (1/32): /path/to/file1
 Status: Optimizing (3/32): /path/to/file2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* dbcompact uses the native db4 mode when talking to libdb4 */
#include "db.h"

/* use a db cache size of 8MB */
#define COMPACT_CACHESIZE (8*1024*1024)

static void ci_print(DB_COMPACT *ci)
{
	printf("pages_free: %i\n", ci->compact_pages_free);
	printf("pages_examine: %i\n", ci->compact_pages_examine);
	printf("pages_levels: %i\n", ci->compact_levels);
	printf("pages_truncated: %i\n", ci->compact_pages_truncated);
}


static int do_compact(DB *db, DB_COMPACT *compactinfo, int flags)
{
	int ret;
	const char *filename;
	const char *dbname;

        db->get_dbname(db, &filename, &dbname);
	if((ret=db->compact(db, NULL, NULL, NULL, compactinfo, flags, NULL))) {
		db->err(db, ret, "error while compacting db, next");
		db->close(db, 0);
                return ret;
	}

	return(ret);
}


int main(int ac, char **dc)
{
	DB *db;
	DB_COMPACT compactinfo;

	int notused=0, flags=0;
	int pass=1, totalsteps=0, filenr=0;
        int opt_truncated_pages=0;

	if(ac == 1) {
		printf("usage: db_compact db_file\n");
                exit(2);
	}

	/* # of files * 2 = total passes to be done */
	totalsteps=(ac-1)*2;

	for(filenr=1; filenr < ac; filenr++) {
                opt_truncated_pages=0;
		memset(&compactinfo, 0, sizeof(compactinfo));

		if(db_create(&db, NULL, 0)) {
			printf("error on db_create\n");
			goto realout;
		}

		if(db->set_cachesize(db, 0, COMPACT_CACHESIZE, 1)) {
			printf("error on ->set_cachesize()\n");
			goto out;
		}

		if(db->open(db, NULL, dc[filenr], NULL, DB_UNKNOWN, 0, 0)) {
			printf("error on db_open with %s\n", dc[filenr]);
			goto out;
		}


		/* 1st pass */
		printf("Status: Compacting(%i/%i) %s\n", pass, totalsteps, dc[filenr]);
		do_compact(db, &compactinfo, DB_FREE_SPACE);
                opt_truncated_pages += compactinfo.compact_pages_truncated;

		/* 2nd pass */
		pass++;
		printf("Status: Compacting(%i/%i) %s\n", pass, totalsteps, dc[filenr]);
		do_compact(db, &compactinfo, DB_FREE_SPACE);
		opt_truncated_pages += compactinfo.compact_pages_truncated;

		db->close(db, 0);
		printf("Result: %d pages truncated\n", opt_truncated_pages);
		pass++;
	}

	exit(0);

out:
	db->close(db, 0);
realout:
	printf("BAILING OUT REALLY BAD\n");
	exit(1);
}