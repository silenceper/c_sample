CREATE TABLE alias (name TEXT, alias TEXT);

CREATE TABLE config (id INTEGER PRIMARY KEY, db_version INTEGER, last_update INTEGER, has_new INTEGER, last_check INTEGER);

INSERT INTO config (last_update, has_new, last_check) VALUES (0, 0, 0);

CREATE TABLE universe (name TEXT, generic_name TEXT, is_desktop INTEGER, category TEXT, arch TEXT, version TEXT, exec TEXT, rir INTEGER, priority TEXT, install TEXT, license TEXT, homepage TEXT, source TEXT, repo TEXT, size INTEGER, sha TEXT, build_date INTEGER, packager TEXT, uri TEXT, description TEXT,installed INTEGER DEFAULT 0, can_update INTEGER DEFAULT 0, data_count INTEGER, PRIMARY KEY(name,version,repo,source));

CREATE TABLE universe_data (name TEXT, version TEXT, source TEXT, repo TEXT, data_name TEXT, data_format TEXT, data_size INTEGER, data_install_size INTEGER, data_depend TEXT, data_bdepend TEXT, data_recommended TEXT, data_conflict TEXT, data_replace TEXT);

CREATE TABLE world (name TEXT, generic_name TEXT, is_desktop INTEGER, category TEXT, arch TEXT, version TEXT, exec TEXT, rir INTEGER, priority TEXT, install TEXT, license TEXT, homepage TEXT, repo TEXT, size INTEGER, sha TEXT, build_date INTEGER, packager TEXT, uri TEXT, description TEXT, can_update INTEGER DEFAULT 0, version_available TEXT, data_count INTEGER, install_time INTEGER, PRIMARY KEY(name));

CREATE TABLE world_data (name TEXT, version TEXT, data_name TEXT, data_format TEXT, data_size INTEGER, data_install_size INTEGER, data_depend TEXT, data_bdepend TEXT, data_recommended TEXT, data_conflict TEXT, data_replace TEXT);

CREATE TABLE world_file (name TEXT, version TEXT, file TEXT , type TEXT, size INTEGER, perms TEXT, uid INTEGER, gid INTEGER, mtime INTEGER, extra TEXT, replace TEXT, replace_with TEXT);

CREATE TABLE keywords (name TEXT, language TEXT, kw_name TEXT, kw_generic_name TEXT, kw_fullname TEXT, kw_comment , PRIMARY KEY(name,language) );

CREATE TABLE source ( id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, repo TEXT, last_update INTEGER, last_check INTEGER, has_new INTEGER, checksum TEXT );

CREATE INDEX universe_category ON universe ( category );
CREATE INDEX universe_data_name ON universe_data ( name );
CREATE INDEX world_data_name ON world_data ( name );
CREATE INDEX world_file_name ON world_file ( name );
CREATE INDEX world_file_file ON world_file ( file );
