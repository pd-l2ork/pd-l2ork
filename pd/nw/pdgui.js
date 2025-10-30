"use strict";

var pwd;
var lib_dir;
var help_path, browser_doc, browser_path, browser_init;
var autocomplete, autocomplete_prefix, autocomplete_relevance;
var pd_engine_id;


exports.autocomplete_enabled = function() {
    return autocomplete;
}

exports.set_pwd = function(pwd_string) {
    pwd = pwd_string;
}

exports.get_pwd = function() {
    return pwd;
}

function defunkify_windows_path(s) {
    var ret = s;
    if (process.platform === "win32") {
        ret = ret.replace(/\\/g, "/");
    }
    return ret;
}

function funkify_windows_path(s) {
    var ret = s;
    if (process.platform === "win32") {
        ret = ret.replace(/\//g, "\\");
    }
    return ret;
}

exports.funkify_windows_path = funkify_windows_path;

exports.set_pd_engine_id = function (id) {
    //console.log("set_pd_engine_id " + id);
    pd_engine_id = id;
}

exports.defunkify_windows_path = defunkify_windows_path;

function gui_set_browser_config(doc_flag, path_flag, init_flag,
                                ac_flag, ac_prefix_flag, ac_relevance_flag,
                                helppath) {
    // post("gui_set_browser_config: " + helppath.join(":"));
    browser_doc = doc_flag;
    browser_path = path_flag;
    browser_init = init_flag;
    help_path = helppath;
    // AG: This should ideally be in its own separate callback, but for the
    // time being we really need the tie-in with the help browser here, so
    // that the completion index can be loaded first, and then be updated with
    // data from the help index when make_index() gets called. Also note that
    // in order to keep things simple, we build the autocompletion index even
    // if autcompletion is disabled, so that the index is ready to go if the
    // user decides to enable it later.
    autocomplete = ac_flag;
    autocomplete_prefix = ac_prefix_flag;
    autocomplete_relevance = ac_relevance_flag;
    make_completion_index();
    // AG: Start building the keyword index for dialog_search.html. We do this
    // here so that we can be sure that lib_dir and help_path are known
    // already. (This may also be deferred until the browser is launched for
    // the first time, depending on the value of browser_init, unless
    // autocomplete is enabled in which case we have to build it anyway.)
    if (autocomplete == 1 && !fs.existsSync(expand_tilde(compl_name))) {
        // if the completion.json file has gone missing, rebuild it
        rebuild_index();
    } else if (browser_init == 1 || autocomplete == 1) {
        // otherwise we only generate the index as needed
        make_index();
    }
}

function gui_set_lib_dir(dir) {
    post("lib_dir=" + dir);
    lib_dir = dir;
}

exports.get_lib_dir = function() {
    return lib_dir;
}

function get_pd_opendir() {
    if (pd_opendir) {
        return pd_opendir;
    } else {
        return pwd;
    }
}

exports.get_pd_opendir = get_pd_opendir;

function set_pd_opendir(dir) {
    pd_opendir = dir;
}

function gui_set_current_dir(dummy, dir_and_filename) {
    set_pd_opendir(path.dirname(dir_and_filename));
}

function gui_set_gui_preset(name) {
    //post("gui_set_gui_preset " + name);
    skin.set(name);
}

exports.set_focused_patchwin = function(cid) {
    //post("set_focused_patchwin=" + cid);
    last_focused = cid;
}

// ico@vt.edu 2020-08-12: check which OS we have since Windows has a different
// windows positioning logic on nw.js 0.14.7 than Linux/OSX. Go figure...
function check_os(name) {

    var os = require('os');
    //post("os=" + os.platform());
    return os.platform() === name ? 1 : 0;
}

exports.check_os = check_os;

// ico@vt.edu 2020-08-14: used to speed-up window size consistency and other
// OS-centric operations by avoiding constant calls to string comparisons.
// Most pertinent calls can be found in index.js' nw_create_window and pdgui.js'
// canvas_check_geometry
var nw_os_is_linux = check_os("linux");
var nw_os_is_osx = check_os("darwin");
var nw_os_is_windows = check_os("win32");

exports.nw_os_is_linux = nw_os_is_linux;
exports.nw_os_is_osx = nw_os_is_osx;
exports.nw_os_is_windows = nw_os_is_windows;

// Keyword index (cf. dialog_search.html)

var fs = require("fs");
var path = require("path");
var dive = require("./dive.js"); // small module to recursively search dirs
var elasticlunr = require("./elasticlunr.js"); // lightweight full-text search engine in JavaScript, cf. https://github.com/weixsong/elasticlunr.js/
elasticlunr.clearStopWords();

function init_elasticlunr()
{
    index = elasticlunr();
    index.addField("title");
    index.addField("keywords");
    index.addField("description");
    index.addField("related_objects");
    index.addField("ref_related_objects");
    index.addField("inlets_outlets");
    index.addField("tippathname");
    index.setRef("id");
    return index;
}

var index = init_elasticlunr();
var index_cache = new Array();
var index_manif = new Set();

function index_entry_esc(s) {
    if (s) {
	var t = s.replace(/\\/g, "\\\\").replace(/:/g, "\\:");
	return t.replace(/(?:\r\n|\r|\n)/g, "\\n");
    } else {
	return "";
    }
}

// GB: This actually retrieves the meta data concerning related_objects,
// keywords, and description of help patches.
function add_doc_details_to_index(filename, data) {
    //post(filename);
    var title = path.basename(filename, "-help.pd"),
        // AG: This is confusing. I'm not sure why we just replace the first
        // newline here. Maybe there's a reason to do so, but I don't get it.
        big_line = data.replace("\n", " "),
        keywords,
        desc,
        rel_objs,
        ref_rel_objs,
        inout,
        tippathname;  //added
    // GB: investigate the related objects of each file
    const machine_related_objects = {
        found_objects: [],
        ref_found_objs: [],
        state: 'SEARCHING_CANVAS',
        transitions: {
            SEARCHING_CANVAS: {
                search_regex: function(line_string) {
                    if (/#N canvas \-?\d+ \-?\d+ \-?\d+ \-?\d+ Related_objects \-?\d+;/i.test(line_string)) {
                        this.state = "SEARCHING_OBJS";
                    }
                },
            },
            SEARCHING_OBJS: {
                search_regex: function(line_search) {
                    var rel_obj = line_search.match(/#X obj \-?\d+ \-?\d+ (pddp\/helplink )?(\w*\/)?([\w|\~]+);/i);
                    if (rel_obj != null) {
                        this.adds(rel_obj[3]);
                    } else if (/#X restore \-?\d+ \-?\d+ pd Related_objects;/i.test(line_search)) {
                        this.state = "SEARCHING_CANVAS";
                    } else if ((/#N canvas \-?\d+ \-?\d+ \-?\d+ \-?\d+ [\S]* \-?\d+;/i.test(line_search))) {
                        if (title != "random") {
                            this.adds("pd");
                        }
                        this.state = "PD_AS_REL_OBJ";
                    }
                },
            },
            PD_AS_REL_OBJ: {
                search_regex: function(line_search) {
                    if (/#X restore \-?\d+ \-?\d+ pd;/i.test(line_search)) {
                        this.state = "SEARCHING_OBJS";
                    }
                }
            }
        },
        dispatch(actionName, line_string) {
            const action = this.transitions[this.state][actionName];
            if (action) {
                action.call(this, line_string);
            }
        },
        adds(obj_title) {
            if (!this.found_objects.includes(obj_title)) {
                let obj_path = index.search(obj_title, {fields: {title: {}}});
                if (obj_path.length > 0) obj_path = obj_path[0].ref;
                this.found_objects.push(obj_title);
                this.ref_found_objs.push(obj_path);
            }
        },
    };
    rel_objs = Object.create(machine_related_objects);
    let eval_string = data.split("\n");
    eval_string.forEach(function(l,a,i) {
        rel_objs.dispatch('search_regex', l.toString().replace(/\(/g, "\\\(")
            .replace(/\)/, "\\\)"));
    });
    ref_rel_objs = rel_objs.ref_found_objs.toString();
    rel_objs = rel_objs.found_objects;
    rel_objs = rel_objs ? rel_objs.toString().replace(/\,/g, " ") : null;

    // We use [\s\S] to match across multiple lines...

    keywords = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ KEYWORDS ([\s\S]*?);/i);
    desc = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ DESCRIPTION ([\s\S]*?);/i);

    var inlet = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ INLET_. ([\s\S]*?);/gi);

    var outlet = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ OUTLET_. ([\s\S]*?);/gi);


    keywords = keywords && keywords.length > 1 ? keywords[1].trim() : null;
    desc = desc && desc.length > 1 ? desc[1].trim() : null;

    if (nw_os_is_windows) {
        // we don't need an RPI-LATER workaround here since this is Windows
        tippathname=filename.replaceAll("\\"," ");
    } else {
        // we check for the RPi nwjs version since as of 2023-08-14 that one
        // lacks the replaceAll function, and provide our own
        // RPI-LATER: update this once we update the RPi nwjs version to a newer one
        if (check_nw_version("0.28"))
            tippathname = String(filename).split("/").join(" ");
        else
            tippathname=filename.replaceAll("/"," ");
    }
    //post("tippathname=" + tippathname);
    // inlet = inlet && inlet.length > 1 ? inlet[1].trim() : null;

    // Remove the Pd escapes for commas
    // AG: We want to do a global replacement here.
    desc = desc ? desc.replace(/ \\,/g, ",") : null;
    if (desc) {
        // AG: And we want to get rid of the newlines, there's no reason
        // whatsover to preserve the original formatting.
        desc = desc.replace(/\n/g, " ");
    }
    // console.log("title: "+ title);
    // console.log("inlet: "+ inlet);
    // console.log("outlet: "+outlet);
    let res = '';
    if(inlet){
        if(inlet.length !=0){
            res+="|";
        }
        for (let i = 0; i < inlet.length; i++) {
            const match = inlet[i].match(/INLET_+([\s\S]*?);/i);
            // console.log(match);

            if (match){
                res +=  "I"+ match[1].trim()+ '|';
              }
        }
    }
    if(outlet){
        if(outlet.length !=0 && !inlet){
            res+="|";
        }
        for (let i = 0; i < outlet.length; i++) {
            const match2 = outlet[i].match(/OUTLET_+([\s\S]*?);/i);

            if ( match2){
                res +="O"+ match2[1].trim()+ '|';
              }
        }
    }
    // console.log("title: "+ title);
    // console.log("inlet: "+ inlet);
    //console.log("res: "+res);
    // console.log("outlet: "+outlet);
    inout = res;
    // AG: And we want to get rid of the newlines, there's no reason
    // whatsover to preserve the original formatting.
    inout = inout.replace(/\n/g, " ");
    // outlet = outlet.replace(/\n/g, " ");
    // console.log (title + ' ' + inout);

    // AG: Deal with a bunch of special cases which have multiple objects
    // documented in them, listing the object names (and arguments) in the
    // NAME field of the META data.
    var names = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ NAME ([\s\S]*?);/);
    // Some NAME fields span multiple lines, pretend that they're spaces.
    names = names && names.length > 1
        ? names[1].trim().replace(/\n/g, " ").split(" ")
        : [];
    if (names.length > 1) {
        // special help file, not a single object, remove from completions
        let obj_result = obj_exact_match(title);
        if (obj_result.length !== 0) {
            let obj_ref = obj_result[0].refIndex;
            //post("scanning "+filename);
            completion_index.removeAt(obj_ref);
        }
        // Some NAME entries (e.g., list) list different variations of the
        // same object (list append, list trim etc.), we try to be clever
        // about these.
        let prefix = names[0];
        let count = names.filter(name => name == prefix).length;
        if (count > 3) {
            // Chances are good that we're looking at variations of the same
            // command, and not just some oddball NAME list with repeated
            // entries. First collect the arguments.
            let myargs = [];
            for (let i = 0; i < names.length; i++) {
                if (names[i] == prefix) {
                    let a = [];
                    while (i+1 < names.length && names[i+1] != prefix) {
                        a.push(names[i+1]); i++;
                    }
                    if (a.length > 0) {
                        myargs.push({"occurrences" : 0, "text" : a.join(' ')});
                    }
                }
            }
            // Next check for an existing object which can be updated.
            obj_result = obj_exact_match(prefix);
            if (obj_result.length !== 0) {
                // Found an object, add new arg entries in-place.
                let args = obj_result[0].item.args;
                for (let i = 0; i < myargs.length; i++) {
                    let arg_result = arg_exact_match(title, myargs[i].text);
                    if (arg_result.length == 0) {
                        // New arg completion. Note that the args array is
                        // live data straight from the fuse, so we can just
                        // add it in-place.
                        args.push(myargs[i]);
                    } else {
                        // keep track of which args we actually added
                        myargs[i] = null
                    }
                }
            } else {
                // the object doesn't exist yet in the index, add it
                completion_index.add({
                    "occurrences" : 0, "title" : prefix, "args" : myargs
                });
            }
            //post("add completion "+prefix+" "+myargs.filter(a => a != null).map(a => a.text).join(', '));
        } else {
            // just a list of object names here, add them all
            for (let i = 0; i < names.length; i++) {
                let title = names[i];
                // check for existing entries, skip those
                obj_result = obj_exact_match(title);
                if (obj_result.length === 0) {
                    //post("add completion "+title);
                    completion_index.add({
                        "occurrences" : 0,
                        "title" : title,
                        "args" : []
                    });
                }
            }
        }
    }

    /* AG: Some help patches have library information in them, we might want
       to use that information, so we record it in the completion entries if
       it is available. NOTE: The lib field will only be added to entries for
       which library information is available, so you *must* check for its
       existence before trying to access that data.*/
    var libs = big_line
        .match(/#X text \-?[0-9]+ \-?[0-9]+ LIBRARY ([\s\S]*?);/);
    libs = libs && libs.length > 1
        ? libs[1].trim().replace(/\n/g, " ").split(" ")
        : [];
    // Filter out some unwanted noise.
    libs = libs.filter(x => x!="external" && x!="addons");
    if (libs.length > 0) {
        //post(title+" lib: "+libs.join(' '));
        let obj_result = obj_exact_match(title);
        if (obj_result.length !== 0) {
            // The first LIBRARY entry seems to be most useful.
            obj_result[0].item.lib = libs[0];
        }
    }

    if (title.slice(-5) === "-help") {
        title = title.slice(0, -5);
    }
    index_cache[index_cache.length] = [filename, title, keywords, desc, rel_objs, ref_rel_objs, inout, tippathname]
	.map(index_entry_esc).join(";:");
    var d = path.dirname(filename);
    index_manif.add(d);
    // Also add the parent directory to catch additions of siblings.
    index_manif.add(path.dirname(d));
    index.update({
        "id": filename,
        "title": title,
        "keywords": keywords,
        "description": desc,
        "related_objects": rel_objs,
        "ref_related_objects": ref_rel_objs,
        "inlets_outlets": inout,
        "tippathname": tippathname
        //"body": big_line,
    });
}

// GB: This does an initial scan of help patches, recording filename, title and
// parent dir, without looking at the meta data.
function add_doc_to_index(err, filename, stat) {
    //post("add_doc_to_index " + err + " " + filename + " " + stat);
    if (!err) {
        if (filename.slice(-8) === "-help.pd") {
            try {
                let title = path.basename(filename, "-help.pd");
                index.addDoc({
                    "id": filename,
                    "title": title
                })
                // ico 2023-11-20: EXPERIMENTAL addition of prefix to
                // objects that are not in the extra path to ensure they
                // are successfully created regardless the pd-l2ork's
                // path option
                let completion_title = defunkify_windows_path(filename).
                                       replace(lib_dir + "/extra/", "").
                                       slice(0, -8);;
                //post("completion_title=" + completion_title);
                if (obj_exact_match(title).length===0) {
                    completion_index.add({
                        "occurrences" : 0,
                        "title" : completion_title,
                        "args" : []
                    });
                }
            } catch (read_err) {
                post("add_doc_to_index err: " + read_err);
            }
        }
    } else {
        // AG: Simply ignore missing/unreadable files and directories.
        // post("err: " + err);
    }
}

var index_done = false;
var index_started = false;
var index_start_time;

// Filenames for the index cache, relative to the user's homedir.
const cache_basename = nw_os_is_windows
      ? "~/AppData/Roaming/Pd-L2Ork/"
      : "~/.pd-l2ork/";
const cache_name = cache_basename + "search.index";
const stamps_name = cache_basename + "search.stamps";
const compl_name = cache_basename + "completions.json";

function finish_index() {
    index_done = true;
    var have_cache = index_cache.length > 0;
    try {
	// write the index cache if we have one
	if (have_cache) {
	    var a = new Array();
	    index_manif.forEach(function(x) {
		var st = fs.statSync(x);
		a[a.length] = index_entry_esc(x) + ";:" + st.mtimeMs;
	    });
	    a.sort();
	    // Make sure that the target dir exists:
	    try {
		fs.mkdirSync(expand_tilde(path.dirname(cache_name)));
	    } catch (err) {
		//console.log(err);
	    }
	    fs.writeFileSync(expand_tilde(cache_name),
			     index_cache.join("\n"), {mode: 0o644});
	    // also write a manifest with the timestamps of all directories:
	    fs.writeFileSync(expand_tilde(stamps_name),
			     a.join("\n"), {mode: 0o644});
	}
    } catch (err) {
	console.log(err);
    }
    var t = new Date().getTime() / 1000;
    setTimeout(function() {
        post("finished " + (have_cache?"building":"loading") + " help index (" +
             (t-index_start_time).toFixed(2) + " secs)");
    }, 100);

    //post("loading file from the disk");
    var idxf = fs.readFileSync
        (expand_tilde(cache_name), 'utf8');
    index_file_lines = idxf.split('\n');
    index_file_lines_path = idxf.split('\n');
    for (var i = 0; i < index_file_lines.length; i++) {
        index_file_lines_path[i] =
            index_file_lines_path[i].split(';:').pop();
    }
    //now redraw all canvases to update tooltips
    redraw_all_visible_canvases();
}

// AG: pilfered from https://stackoverflow.com/questions/21077670
function expand_tilde(filepath) {
    if (filepath[0] === '~') {
        var home = nw_os_is_windows ? process.env.USERPROFILE : process.env.HOME;
        return path.join(home, filepath.slice(1));
    }
    return filepath;
}

function check_timestamps(manif)
{
    manif = manif.split('\n');
    for (var j = 0, l = manif.length; j < l; j++)
    {
    	if (manif[j])
        {
    	    var e = manif[j].replace(/\\:/g, "\x1c").split(';:')
    		.map(x => x
                .replace(/\x1c/g, ":")
                // ico@vt.edu 2024-12-16: the following breaks on \\neural where
                // \n is misinterpreted as line break. since no other paths
                // currently have line breaks, we can safely remove this altogether
                //.replace(/\\n/g, "\n")
                .replace(/\\\\/g, "\\"));
    	    var dirname = e[0] ? e[0] : null;
    	    var stamp = e[1] ? parseFloat(e[1]) : 0.0;
    	    try
            {
                var st = fs.statSync(dirname);
                if (st.mtimeMs > stamp)
                {
                    return false;
                }
    	    } catch (err)
            {
        		return false;
                //post("...need to rebuild due to error dirname=" + dirname + " stamp=" + stamp);
    	    }
    	}
    }
    return true;
}

// AG: This is normally executed only once, after lib_dir has been set.
// Note that dive() traverses lib_dir asynchronously, so we report back in
// finish_index() when this is done.
function make_index(force) {
    var doc_path = browser_doc?path.join(lib_dir, "doc"):lib_dir;
    var i = 0;
    var l = help_path.length;
    function detail_files() {
        let all_indexed_files = Object.keys(index.documentStore.docs);
        var data;
        all_indexed_files.forEach(function(filename,i,a) {
            // AG: We MUST read the files synchronously here. This might be a
            // performance issue on some systems, but if we don't do this then
            // we may open a huge number of files simultaneously, causing the
            // process to run out of file handles.
            try {
                data = fs.readFileSync(filename, { encoding: "utf8", flag: "r" });
                add_doc_details_to_index(filename, data);
            } catch (read_err) {
                post("make_index err: " + read_err);
            }
        });
        finish_index();
    }
    function make_index_cont() {
        if (browser_path && i < l) {
            var doc_path = help_path[i++];
            // AG: These paths might not exist, ignore them in this case. Also
            // note that we need to expand ~ here.
            var full_path = expand_tilde(doc_path);
            fs.lstat(full_path, function(err, stat) {
                if (!err) {
                    post("scanning help patches in " + full_path);
                    dive(full_path, add_doc_to_index, make_index_cont);
                } else {
                    make_index_cont();
                }
            });
        } else {
            // reset the help path index, then invoke the main pass
            i = 0;
            post("building help index...");
            detail_files();
        }
    }
    pdsend("pd gui-busy 1");
    index_started = true;
    index_start_time = new Date().getTime() / 1000;
    var idx, manif;
    try {
        // test for index cache and manifest
        idx = fs.readFileSync
        (expand_tilde(cache_name), 'utf8');
        manif = fs.readFileSync
        (expand_tilde(stamps_name), 'utf8');
    } catch (err) {
        //console.log(err);
    }
    // ico@vt.edu 2022-11-09: force rebuilding of index if
    // browser_init option is set in the general preferences tab
    // OR (2022-11-26) if we are passing a "force" flag (used by
    // the preferences "Rebuild Index" button)
    if (!force && idx && manif && check_timestamps(manif) && !browser_init) {
        // index cache is present and up-to-date, load it
        setTimeout(function() {
            post("\nloading cached help index from " + cache_name);
        }, 100);
        idx = idx.split('\n');
        for (var j = 0, l = idx.length; j < l; j++) {
            if (idx[j]) {
                var e = idx[j].replace(/\\:/g, "\x1c").split(';:')
                    .map(x => x
                        .replace(/\x1c/g, ":")
                        .replace(/\\n/g, "\n")
                        .replace(/\\\\/g, "\\"));
                var filename = e[0] ? e[0] : null;
                var title = e[1] ? e[1] : null;
                var keywords = e[2] ? e[2] : null;
                var desc = e[3] ? e[3] : null;
                var rel_obj = e[4] ? e[4] : null;
                var ref_rel_obj = e[5] ? e[5] : null;
                var inout = e[6] ? e[6] : null;
                var tippathname = e[7] ? e[7] : null;
                index.addDoc({
                    "id": filename,
                    "title": title,
                    "keywords": keywords,
                    "description": desc,
                    "related_objects": rel_obj,
                    "ref_related_objects": ref_rel_obj,
                    "inlets_outlets": inout,
                    "tippathname":tippathname
                    // "outlets": outlet
                });
            }
        }
        finish_index();
    } else {
        // no index cache, or it is out of date, so (re)build it now, and
        // save the new cache along the way
        setTimeout(function() {
            var idx, manif, prefix = "";
            try {
                // test for index cache and manifest
                idx = fs.readFileSync(expand_tilde(cache_name), 'utf8');
                manif = fs.readFileSync(expand_tilde(stamps_name), 'utf8');
            } catch (err) {
                //console.log(err);
            }
            if (manif)
            {
                prefix = "re";
            }
            post("\n" + prefix + "building search index...\nthis may take a " +
                 "while (up to 2 minutes, or so) and will have to be done " +
                 "only once, unless you alter indexing options in the " +
                 "preferences window's General tab.\n" +
                 "scanning help patches in " + doc_path + "...");
            setTimeout(function() {
                dive(doc_path, add_doc_to_index, make_index_cont);
            }, 100);
        }, 500);
    }
    // ico 2023-07-22:
    // finally load the indexed file into working memory
    // for old school manual exact search to be used
    // for tooltips (this one does not require redrawing
    // because this code path is reached only if the
    // file already exists)
    //post("loading file from the disk");
    var idxf = fs.readFileSync
        (expand_tilde(cache_name), 'utf8');
    index_file_lines = idxf.split('\n');
    index_file_lines_path = idxf.split('\n');
    for (var i = 0; i < index_file_lines.length; i++) {
        index_file_lines_path[i] =
            index_file_lines_path[i].split(';:').pop();
    }
    pdsend("pd gui-busy 0");
}

// AG: This is called from dialog_search.html with a callback that expects to
// receive the finished index as its sole argument. We also build the index
// here if needed, using make_index, then simply wait until make_index
// finishes and finally invoke the callback on the resulting index.
function build_index(cb) {
    function build_index_worker() {
        if (index_done == true) {
            cb(index);
        } else {
            setTimeout(build_index_worker, 500);
        }
    }
    if (index_started == false) {
        make_index();
    }
    build_index_worker();
}

exports.build_index = build_index;

// normally, this doesn't actually rebuild the index, it just clears it, so
// that it will be rebuilt the next time the help browser is opened
// then, there is the force option, that also rebuilds it right away
function rebuild_index(force)
{
    //post("rebuild_index force=" + force);
    index = init_elasticlunr();
    index_cache = new Array();
    index_manif = new Set();
    index_started = index_done = false;
    try {
    	fs.unlink(expand_tilde(cache_name));
    	fs.unlink(expand_tilde(stamps_name));
    } catch (err) {
	    //console.log(err);
    }
    if (browser_init == 1 || autocomplete == 1 || force) {
        // if autocomplete is enabled, we *have* to rebuild the index now
        //post("rebuild_index calling make_index force=" + force);
        make_index(force);
    } else {
        // we can defer rebuilding of the index until the browser is reopened
        post("clearing help index (reopen the browser to rebuild!)");
    }
}

exports.rebuild_index = rebuild_index;

// this is called from the gui and general tabs of the prefs dialog
function update_browser(doc_flag, path_flag, ac_flag, ac_prefix_flag, ac_relevance_flag, force)
{
    //post("update_browser force=" + force);
    var changed = ac_flag == 1 && autocomplete == 0;
    autocomplete = ac_flag;
    autocomplete_prefix = ac_prefix_flag;
    autocomplete_relevance = ac_relevance_flag;
    doc_flag = doc_flag? 1 : 0;
    path_flag = path_flag? 1 : 0;
    if (browser_doc !== doc_flag) {
	   browser_doc = doc_flag;
	   changed = true;
    }
    if (browser_path !== path_flag) {
	   browser_path = path_flag;
	   changed = true;
    }
    if (changed || force) {
        if (force)
	        rebuild_index(force);
        else
            rebuild_index();
    }
}

exports.update_browser = update_browser;

// GB: autocompletion feature

const fuse = require("./fuse.js");
let autocomplete_index_options = {
    useExtendedSearch : true,
    includeMatches : true,
    includeScore : true,
    keys : ["title", "args.text"]
};
let args_completion_field = {keys : ["args.text"]};
let objs_completion_field = {keys : ["title"]};
var completion_list = [], completion_index;

function make_completion_index() {
    try {
        completion_list = require(expand_tilde(compl_name));
    } catch (e) {
        post("No completion list found");
    }
    completion_index = new fuse(completion_list,autocomplete_index_options);
}

// GB: fuse legend for expand search
// "=text" search exact match for 'text'
// "'text" search match that contain 'text'
// "^text" search match that begins with 'text'
// "!text" search match that doesn't contain 'text'

function obj_exact_match(title) {
    return completion_index.search("=\"" + title + "\"", objs_completion_field);
}

function search_obj(title) {
    if (!autocomplete) return [];
    // GB approaches: search objects that *contains* search _word_ (1st line) or patches that *begins with* _word_ (2nd line)
    // either should skip the result 'message' and 'text' thus messages and comments are not created the same way objects are
    if (!autocomplete_prefix) {
        return completion_index.search({$and: [{"title": "'\"" + title + "\""}, {"title": "!\"message\"" }, {"title": "!\"text\"" }]});
    } else {
        return completion_index.search({$and: [{"title": "^\"" + title + "\""}, {"title": "!\"message\"" }, {"title": "!\"text\"" }]});
    }
}

function arg_exact_match(title, arg) {
    return completion_index.search({$and: [{"title": "=\"" + title + "\""}, {"args.text": "=\"" + arg + "\""}]});
}

function search_arg(title, arg) {
    if (!autocomplete) return [];
    // for the arguments, we are only interested on the obj that match exactly the 'title', so we return only the args from this obj
    let results = completion_index.search({$and: [{"title": "=\"" + title + "\""}, {"args.text": "^\"" + arg + "\""}]});
    if (results.length > 0) {
        let args = results[0].item.args;
        // AG: Matched args are in matches.slice(1,), extract them.
        // This code originally just returned matches itself, which has the
        // text of all matched arguments, but not the occurrence data, and we
        // need the latter to sort based on relevance.
        results = results[0].matches.slice(1,).map(a => args[a.refIndex]);
    }
    return results;
}

function search_args(title) {
    // like above, but look up *all* arg completions for a given object
    if (!autocomplete) return [];
    // for the arguments, we are only interested on the obj that match exactly the 'title', so we return only the args from this obj
    let results = obj_exact_match(title);
    // item.args is live data from the fuse, make sure to take a shallow copy
    // with slice() which can be safely sorted in-place later
    return (results.length > 0) ? results[0].item.args.slice() : [];
}

function index_obj_completion(obj_or_msg, obj_or_msg_text) {
    //post("index_obj_completion " + obj_or_msg + " " + obj_or_msg_text);
    var title, arg;
    if (obj_or_msg === "obj") {
        let text_array = obj_or_msg_text.split(" ");
        title = text_array[0];
        arg = text_array.slice(1, text_array.length).toString().replace(/\,/g, " ");
    } else { // the autocomplete feature doesn't work with messages and comments
        return;
    }
    var obj_ref, obj_freq = 1, args = [], arg_ref = 0, arg_freq = 1, obj_found = false;
    let obj_result = obj_exact_match(title);
    if (obj_result.length !== 0) {
        obj_found = true;
        obj_ref = obj_result[0].refIndex;
        obj_freq = obj_result[0].item.occurrences + 1;
        args = obj_result[0].item.args;
        /*
        post("index_obj_completion\n  obj_ref=" + obj_ref +
             "\n  obj_freq=" + obj_freq +
             "\n  args=" + args);
        */
        if (arg) {
            arg_ref = args.length;
            let arg_result = arg_exact_match(title, arg);
            if (arg_result.length !== 0) {
                arg_ref = arg_result[0].matches[1].refIndex;
                /*
                post("...\n  arg=" + arg +
                     "\n  args.length=" + args.length +
                     "\n  arg_result=" + arg_result +
                     "\n  arg_ref=" + arg_ref);
                */
                if (arg_ref !== null) {
                    /*
                    post("...\n  obj_result[0]=" + arg_result[0] +
                         "\n  item=" + arg_result[0].item +
                         "\n  args=" + arg_result[0].item.args +
                         "\n  args.length=" + arg_result[0].item.args.length +
                         "\n  args[arg_ref]=" + arg_result[0].item.args[arg_ref]
                         );
                    */
                    arg_freq = arg_result[0].item.args[arg_ref].occurrences + 1;
                }
            }
        }
    }
    if(arg) args[arg_ref] = {"occurrences" : arg_freq, "text" : arg};
    let obj = {"occurrences" : obj_freq, "title" : title, "args" : args};

    if(obj_found) {
        completion_index.update(obj, obj_ref);
        //post("...updating");
    }
    else {
        completion_index.add(obj);
        //post("...creating a new entry");
    }
}

function write_completion_index() {
    try { // be sure the dir exists
        fs.mkdirSync(expand_tilde(path.dirname(compl_name)));
    } catch (err) {
        // post("err: " + err);
    }
    try { // create the file
        fs.writeFileSync(expand_tilde(compl_name), JSON.stringify(completion_index._docs), {mode: 0o644});
    } catch (err) {
        post("write_completion_index err: " + err);
    }
}

// GB: manage the selection of the autocomplete dropdown options
function update_autocomplete_selected(ac_dropdown, sel, new_sel) {
    if (sel > -1) ac_dropdown.children.item(sel).classList.remove("selected");
    if (new_sel > -1 && new_sel < ac_dropdown.children.length) {
        ac_dropdown.children.item(new_sel).classList.add("selected");
    } else {
        new_sel = -1;
    }
    ac_dropdown.setAttribute("selected_item", new_sel);
}

function update_autocomplete_dd_arrowdown(ac_dropdown) {
    if (ac_dropdown !== null) {
        let above = ac_dropdown.getAttribute("above");
        let sel = ac_dropdown.getAttribute("selected_item");
        if (above == 0) {
            if (sel < ac_dropdown.children.length - 1) {
                update_autocomplete_selected(ac_dropdown, sel, parseInt(sel) + 1);
            }
        } else if (sel != -1) {
            var outcome = parseInt(sel) + 1;
            if (sel == ac_dropdown.children.length - 1) {
                outcome = -1;
            }
            update_autocomplete_selected(ac_dropdown, sel, outcome);           
        }
    }
}

function update_autocomplete_dd_arrowup(ac_dropdown) {
    if (ac_dropdown !== null) {
        let above = ac_dropdown.getAttribute("above");
        let sel = ac_dropdown.getAttribute("selected_item");
        if (above == 0) {
            update_autocomplete_selected(ac_dropdown, sel, parseInt(sel) -
                (above == 0 ? 1 : -1)
            );
        } else {
            if (sel == -1) {
                update_autocomplete_selected(ac_dropdown, sel, ac_dropdown.children.length - 1);
            } else if (sel > 0) {
                update_autocomplete_selected(ac_dropdown, sel, parseInt(sel) - 1);
            }
        }
    }
}

function select_result_autocomplete_dd(textbox, ac_dropdown, last, offs, res, dir) {
    //post("textbox=" + textbox + " dropdown=" + ac_dropdown + 
    //    " last=" + last + " offs=" + offs + " res=" + res + " dir=" + dir);
    if (ac_dropdown !== null) {
        let sel = ac_dropdown.getAttribute("selected_item");
        if (sel > -1) {
            textbox.innerText = ac_dropdown.children.item(sel).innerText;
            delete_autocomplete_dd(ac_dropdown);
            return [sel + offs, offs];
        } else {
        // We only come here if the user presses 'tab' and there is no
        // option selected.
             var n = res.length;
             if (n == 0) {
                 // It seems that while pondering the mysteries of the
                 // universe, your computer has lost our completion list. This
                 // shouldn't happen, but a little bit of defensive programming
                 // can't hurt.
                 return [-1,-1];
             }
             var next =
            (dir==0 ? last : dir>0 ? last+1 : last<=0 ? n-1 : last-1) % n;
            // If the new index is outside the current scope of the popup,
            // repopulate the popup by shifting the entries accordingly (poor
            // man's scroll).
            if (next < offs) {
                offs = next;
                let c = ac_dropdown.childNodes;
                c.forEach((r, i) => r.textContent = res[offs+i]);
            } else if (next > offs+7) {
                offs = next-7;
                let c = ac_dropdown.childNodes;
                c.forEach((r, i) => r.textContent = res[offs+i]);
            }
            textbox.innerText = res[next];
            return [next, offs];
        }
    } else {
        return [-1,-1];
    }
}

// GB: update autocomplete dropdown with new results
function repopulate_autocomplete_dd(cid, doc, ac_dropdown, obj_class, new_obj_element) {
    var text = new_obj_element.innerText;
    ac_dropdown().setAttribute("searched_text", text);
    let title, arg, have_arg;
    if (obj_class === "obj") {
        let text_array = text.split(" ");
        title = text_array[0].toString();
        arg = text_array.slice(1, text_array.length);
        // check whether *anything* follows the obj name, even an empty arg
        have_arg = arg.length !== 0;
        arg = (arg.length !== 0) ? arg.toString().replace(/\,/g, " ") : "";
    } else { // the autocomplete feature doesn't work with messages and comments
        post("repopulate_autocomplete_dd failed");
        return;
    }

    /* AG: We're dealing with three different cases here which must be handled
       separately.

       (1) We're completing an object name; in this case have_arg is false and
       arg is empty as well.

       (2) We're about to start argument completion; here we have that
       have_arg is true even though arg is still empty (which happens as soon
       as you enter a blank after the object name).

       (3) We're in the middle of argument completion (started typing some
       arguments) in which case have_arg is true and arg is non-empty as
       well. */
    let results = (arg.length > 0) ? (search_arg(title, arg)) : have_arg ? (search_args(title)) : (search_obj(title));

    /* AG: Here we massage the result list from what Fuse delivers, which is
       based on scoring similarity and can look pretty random at times. What
       we actually want here is an order which also takes into account
       relevance (as determined by the occurences and library information that
       we have), and the alphabetic order of the available completions. The
       latter tends to make the list tidier and easier to navigate.

       The final step then is to condense the result list to a simple string
       list, since we don't use the other data anymore beyond this point.

       NB: The use of library information was an idea proposed by JW, and we
       record that now, but unfortunately the information available in the
       help patches is incomplete and inconsistent. Notable exceptions are
       'internal' and 'cyclone', for which there are plenty of entries. At
       present we only use 'internal' to identify built-ins, so that they will
       be preferred in relevance ordering. This has the advantage that it
       works from the get-go when there's not much live usage data yet, as
       these objects will tend to be used most frequently by most Pd
       users. But only time and user feedback will tell whether it actually
       makes sense to put this criterion above the live usage data that we
       have in the occurrences field. */
    if (arg.length > 0 || have_arg) {
        // argument completions, these don't have scores, order them by
        // just relevance and text
        results.sort((a, b) =>
            a.occurrences == b.occurrences || !autocomplete_relevance
                ? (a.text == b.text ? 0 : a.text < b.text ? -1 : 1)
                : b.occurrences - a.occurrences);
        results = results.map(a => title + " " + a.text);
    } else {
        // object completions, order by score, relevance and item.title
        results.sort(function (a, b) {
            /* XXXREVIEW: We might have to revisit this. We currently use a
             * numeric relevance score to make this easy to adjust. The boost
             * factor determines the relative weight of built-ins over live
             * usage data. We currently have this at 50, so that you need 50
             * uses of an external to win against a built-in. I hope that this
             * will work well in practice, but currently noone knows. Also,
             * should score take precedence over relevance? Currently we
             * demote it to a secondary criterion, but score can be pretty
             * important if the auto-completion prefix option is disabled
             * (which it is by default). */
            const boost = 50; /* NOTE: Increasing the boost to a very large
                               * value >> 1 makes built-ins effectively take
                               * priority over live usage data, decreasing it
                               * to a very small value << 1 makes live usage
                               * data the most important. */
            let aflag = a.item.hasOwnProperty("lib") && a.item.lib == "internal" ? 1 : 0;
            let bflag = b.item.hasOwnProperty("lib") && b.item.lib == "internal" ? 1 : 0;
            let afreq = a.item.occurrences, bfreq = b.item.occurrences;
            let relevance = (bflag-aflag)*boost + (bfreq-afreq);
            if (autocomplete_relevance && relevance !== 0) {
                return relevance;
            } else if (a.score == b.score) {
                return a.item.title == b.item.title ? 0
                    : a.item.title < b.item.title ? -1 : 1;
            } else {
                let d = a.score - b.score;
                return d == 0 ? 0 : d < 0 ? -1 : 1;
            }
        });
        results = results.map(a => a.item.title);
    }

    // record the complete results, we need them for tab completion
    let all_results = results;
    // GB TODO: ideally we should be able to show all the results in a limited window with a scroll bar
    let n = 8; // Maximum number of suggestions
    if (results.length > n) results = results.slice(0,n);

    ac_dropdown().innerHTML = ""; // clear all old results
    if (results.length > 0) {
        // for each result, make a paragraph child of autocomplete_dropdown
        let h = ac_dropdown().getAttribute("font_height");
        results.forEach(function (f,i,a) {
            let y = h*(i+1);
            let r = doc.createElement("p");
            r.setAttribute("width", "150");
            r.setAttribute("height", h);
            r.setAttribute("y", y);
            r.setAttribute("class", "border");
            r.setAttribute("idx", i);
            r.textContent = f;
            ac_dropdown().appendChild(r);
        })
        ac_dropdown().setAttribute("selected_item", "-1");

        // ico@vt.edu 2022-10-13: make sure that the autocomplete popup can fit
        // below. otherwise, move it above, unless there is not enough room
        // above either.
        var y = ac_dropdown().getBoundingClientRect().y;
        var height = ac_dropdown().offsetHeight;
        var nw_win;
        gui(cid).get_nw_window(function(win) {
            nw_win = win;
        });
        let yoffset = nw_win.window.scrollY;
        let min_y_coord = Number((nw_win.window.document.
            getElementById("patchsvg").getAttribute("viewBox").split(" "))[1]);
        let style = new_obj_element.style;
        let top = parseFloat(style.getPropertyValue("top"));
        /*
        post("y=" + y + " height=" + height + " min_y_coord=" + min_y_coord + 
            " yoffset=" + yoffset + " innerheight=" + nw_win.window.innerHeight);
        post("autocomplete_bottom=" + (y + min_y_coord + height) + " win_bottom=" + 
            (min_y_coord + yoffset + nw_win.window.innerHeight + 3));
        post("obj_top=" + top + "autocomplete_on_top=" + 
            (min_y_coord + yoffset + top - height - 3) + " win_top=" + (min_y_coord));
        post("==> ac_top=" + (top - height - 3 + yoffset));
        */
        if ((y + min_y_coord + height) > 
            (min_y_coord + yoffset + nw_win.window.innerHeight + 3) &&
            (top - height - 3) > 0) {
            //post("does not fit");
            ac_dropdown().style.setProperty("top", (top - height - 3) + "px");
            //post("...final=" + (y - height - 3));
            ac_dropdown().setAttribute("above", "1");
        } else {
            //post("fits");
            // check if the contents have changed to now overlap with the object
            // (in case they were above to begin with)
            if (ac_dropdown().getAttribute("above") == 1) {
                //post("obj_top="+ top + " autocomplete_bottom=" + (y + height));
                ac_dropdown().style.setProperty("top", (y - (y + height + 3 - top)) + "px");
            }
            
        }
        //post("ac_dropdown size=" + ac_dropdown().offsetHeight + 
        //    " x=" + ac_dropdown().getBoundingClientRect().x);
    } else { // if there is no suggestion candidate, the autocompletion dropdown should disappear
        delete_autocomplete_dd (ac_dropdown());
    }
    return all_results;
}

// GB: create autocomplete dropdown based on the properties of the textbox for new_obj_element
function create_autocomplete_dd (cid, doc, ac_dropdown, new_obj_element) {
    /*
    var xoffset, yoffset;
    gui(cid).get_nw_window(function(nw_win) {
        xoffset = nw_win.window.scrollX;
        yoffset = nw_win.window.scrollY;
    });
    */
    if(ac_dropdown === null) {
        let font_width = new_obj_element.getAttribute("font_width");
        let font_height = new_obj_element.getAttribute("font_height");
        let style = new_obj_element.style;
        let font_size = style.getPropertyValue("font-size").toString();
        font_size = parseFloat(font_size.slice(0,font_size.length-2));
        let line_height = style.getPropertyValue("line-height").toString();
        let zoom = parseFloat(line_height.slice(0,line_height.length-1))/100;
        let offset_y = zoom*font_size + 5;
        let top = style.getPropertyValue("top").toString();
        top = parseFloat(top.slice(0, top.length-2)) + offset_y;
        let left = style.getPropertyValue("left").toString();
        left = parseFloat(left.slice(0, left.length-2)) - 1;

        var dd = doc.createElement("div");
        configure_item(dd, {
            id: "autocomplete_dropdown",
            font_width: font_width,
            font_height: font_height
        });

        /*
        var viewBox = doc.getElementById("patchsvg").getAttribute("viewBox").split(" ");
        post("create_autocomplete_dd left=" + parseFloat(style.getPropertyValue("left")) +
            " xoffset=" + xoffset +
            " top=" + top +
            " yoffset=" + yoffset +
            " parentX=" + doc.getElementById("new_object_textentry") +
            " viewBox=" + viewBox
        );
        */

        dd.style.setProperty("position", "absolute");
        dd.style.setProperty("left", (left).toString() + "px");
        dd.style.setProperty("top", (top).toString() + "px");
        dd.style.setProperty("font-size", style.getPropertyValue("font-size"));
        dd.style.setProperty("line-height", line_height);
        dd.style.setProperty("transform", style.getPropertyValue("transform"));
        dd.style.setProperty("max-width", style.getPropertyValue("max-width"));
        // dd.style.setProperty("-webkit-padding-after", style.getPropertyValue("-webkit-padding-after"));
        dd.style.setProperty("min-width", style.getPropertyValue("min-width"));
        dd.setAttribute("selected_item", "-1");
        dd.setAttribute("searched_text", "");
        // ico@vt.edu 2022-10-13:
        // help determine which side of the object we are on
        dd.setAttribute("above", "0");
        // ico@vt.edu 2022-10-12: insert this below the dialogs and scrollbars
        var ref = doc.getElementById("canvas_find");
        ref.parentNode.insertBefore(dd, ref);
    }
}

function delete_autocomplete_dd (ac_dropdown) {
    if (ac_dropdown !== null) {
        ac_dropdown.parentNode.removeChild(ac_dropdown);
    }
}

function check_completion(text)
{
    if (!autocomplete) return false;
    // checks whether an exact match (title+args) exists for the given text
    let text_array = text.split(" ");
    let title = text_array[0].toString();
    let arg = text_array.slice(1, text_array.length);
    if (arg.length > 0) {
        arg = arg.toString().replace(/\,/g, " ");
    } else {
        arg = "";
    }
    let obj_result = obj_exact_match(title);
    if (obj_result.length !== 0) {
        let args = obj_result[0].item.args;
        if (arg !== "") {
            let arg_result = arg_exact_match(title, arg);
            if (arg_result.length !== 0) {
                // found exact title+args match
                return true;
            }
        } else {
            // no args, found exact title match
            return true;
        }
    }
    // no matches found
    return false;
}

function remove_completion(text, log)
{
    //post("remove_completion <" + text+ ">");
    if (!autocomplete) return false;
    //post("...got autocomplete");
    // Check to see whether we're removing an object or just its arguments
    // from the index.
    let text_array = text.split(" ");
    let title = text_array[0].toString();
    let arg = text_array.slice(1, text_array.length);
    if (arg.length > 0) {
        arg = arg.toString().replace(/\,/g, " ");
        //log("title: "+title+", args: "+arg);
    } else {
        arg = "";
        //log("title: "+title);
    }
    let obj_result = obj_exact_match(title);
    if (obj_result.length !== 0) {
        let obj_ref = obj_result[0].refIndex;
        let obj_freq = obj_result[0].item.occurrences;
        let args = obj_result[0].item.args;
        if (arg !== "") {
            let arg_result = arg_exact_match(title, arg);
            if (arg_result.length !== 0) {
                // found exact title+args match
                let arg_ref = arg_result[0].matches[1].refIndex;
                //log("removing %s %s at index %d %d", title, arg, obj_ref, arg_ref);
                // remove from args
                args.splice(arg_ref, 1);
                // update the object
                let obj = {"occurrences" : obj_freq, "title" : title, "args" : args};
                //log("updating %d with %o", obj_ref, obj);
                completion_index.update(obj, obj_ref);
                return true;
            }
        } else {
            // no args, found exact title match, remove it
            //log("removing %s at index %d", title, obj_ref);
            completion_index.removeAt(obj_ref);
            return true;
        }
    }
    // no matches found
    return false;
}

exports.index_obj_completion = index_obj_completion;
exports.write_completion_index = write_completion_index;
exports.update_autocomplete_selected = update_autocomplete_selected;
exports.update_autocomplete_dd_arrowdown = update_autocomplete_dd_arrowdown;
exports.update_autocomplete_dd_arrowup = update_autocomplete_dd_arrowup;
exports.select_result_autocomplete_dd = select_result_autocomplete_dd;
exports.repopulate_autocomplete_dd = repopulate_autocomplete_dd;
exports.create_autocomplete_dd = create_autocomplete_dd;
exports.delete_autocomplete_dd = delete_autocomplete_dd;
exports.check_completion = check_completion;
exports.remove_completion = remove_completion;

// Modules

var cp = require("child_process"); // for starting core Pd from GUI in OSX

var parse_svg_path = require("./parse-svg-path.js");

exports.parse_svg_path = parse_svg_path;

// local strings
var lang = require("./pdlang.js");

exports.get_local_string = lang.get_local_string;

var pd_window;
var pd_window_keys;
// ico@vt.edu 2022-12-10: disabled this
// TODO!: check for regressions
//exports.pd_window;

// Turns out I messed this up. pd_window should really be an
// "nw window", so that you can use it to access all the
// nw window methods and settings.  Instead I set it to the
// DOM window object. This complicates things--for example,
// in walk_window_list I have to take care when comparing
// patchwin[]-- which are nw windows-- and pd_window.
// I'm not sure of the best way to fix this. Probably we want to
// just deal with DOM windows, but that would mean abstracting
// out the stuff that deals with nw window size and
// positioning.
exports.set_pd_window = function(win) {
    pd_window = win;
    exports.pd_window = win;
}

var font_engine_sanity;

// Here we use an HTML5 canvas hack to measure the width of
// the text to check for a font rendering anomaly. Here's why:
//
// It was reported that Ubuntu 16.04, Arch-- and probably most other Gnu/Linux
// distros going forward-- all end up with text extending past the box border.
// The test_text below is the string used in the bug report.
// OSX, Windows, and older Gnu/Linux stacks (like Ubuntu 14.04) all render
// this text with a width that is within half a pixel of each other (+- 217).
//
// Newer versions of Ubuntu and Arch measured nearly 7 pixels wider.
//
// I don't know what the new Gnu/Linux stack is up to (and I don't have the
// time to spelunk) but it's out of whack with the rest of the desktop
// rendering engines. Worse, there's some kind of quantization going on that
// keeps the new Gnu/Linux stack from hitting anything close to the font
// metrics of Pd Vanilla.
//
// Anyhow, we check for the discrepancy and try our best not to make newer
// versions of Gnu/Linux distros look too shitty...
exports.set_font_engine_sanity = function(win) {
    var canvas = win.document.createElement("canvas"),
        ctx = canvas.getContext("2d"),
        test_text = "struct theremin float x float y";
    canvas.id = "font_sanity_checker_canvas";
    win.document.body.appendChild(canvas);
    ctx.font = "11.65px DejaVu Sans Mono";
    if (Math.floor(ctx.measureText(test_text).width) <= 217) {
        font_engine_sanity = true;
    } else {
        font_engine_sanity = false;
    }
    canvas.parentNode.removeChild(canvas);
}

exports.get_font_engine_sanity = function() {
    return font_engine_sanity;
}

function font_stack_is_maintained_by_troglodytes() {
    return !font_engine_sanity;
}

var nw_create_window;
var nw_close_window;
var nw_app_quit;
var nw_open_html;
var nw_open_textfile;
var nw_open_external_doc;

exports.set_new_window_fn = function (nw_context_fn) {
    nw_create_window = nw_context_fn;
}

exports.set_close_window_fn = function (nw_context_fn) {
    nw_close_window = nw_context_fn;
}

exports.set_open_html_fn = function (nw_context_fn) {
    nw_open_html = nw_context_fn;
}

exports.set_open_textfile_fn = function (nw_context_fn) {
    nw_open_textfile = nw_context_fn;
}

exports.set_open_external_doc_fn = function (nw_context_fn) {
    nw_open_external_doc = nw_context_fn;
}

// Global variables from tcl
var pd_myversion,    // Pd version string
    pd_apilist,      // Available Audio APIs (tcl list)
    pd_midiapilist,  // MIDI APIsa (tcl list)
    pd_nt,           // Something to do with Windows configuration
    fontname,        // Font
    fontweight,      //  config
    pd_fontlist,     //   (Seems to be hard coded in Pd-l2ork)
    pd_whichmidiapi, // MIDI API, set by pd->gui message
    pd_whichapi,     // Audio API, set by pd->gui message
    pd_opendir,      //
    pd_guidir,       //
    pd_tearoff,      //
    put_tearoff,     //
    tcl_version,     //
    canvas_fill,     //
    colors,          //
    global_clipboard, //
    global_selection, //
    k12_mode = 0,          // should be set from argv ("0" is just a stopgap)
    k12_menu_vis = 0,  // whether the k12_menu is enabled outside k12 mode
    autotips,          // tooltips
    magicglass,        // cord inspector
    window_prefs,      //retaining window-specific preferences
    pdtk_canvas_mouseup_name, // not sure what this does
    filetypes,         // valid file extensions for opening/saving (includes Max filetypes)
    untitled_number,   // number to increment for each new patch that is opened
    untitled_directory, // default directory where to create/save new patches
    popup_coords,       // 0: canvas x
                        // 1: canvas y
                        // 2: screen x
                        // 3: screen y
    pd_colors = {};                // associative array of canvas color presets

    var pd_filetypes = { ".pd": "Pd Files",
                         ".pat":"Max Patch Files",
                         ".mxt":"Max Text Files",
                         ".mxb":"Max Binary Files",
                         ".help":"Max Help Files"
                       };

    exports.pd_filetypes = pd_filetypes;

    popup_coords = [0,0];

// Keycode vs Charcode: A Primer
// -----------------------------
// * keycode is a unique number assigned to a physical key on the keyboard
// * keycode is device dependent
// * charcode is the ASCII character (printable or otherwise) that gets output
//     when you depress a particular key
// * keydown and keyup events report keycodes but not charcodes
// * keypress events report charcodes but not keycodes
// * keypress events do _not_ fire for non-printing chars like arrow keys,
//     Alt keypress, Ctrl, (possibly) the keypad Delete key, and others
// * in Pd, we want to send ASCII codes + arrow keys et al to Pd for
//     both keydown and keyup events
// * events (without an auto-repeat) happen in this order:
//       1) keydown
//       2) keypress
//       3) keyup
// Therefore are solution is:
// 1. We check for non-printable keycodes like arrow keys inside
//    the keydown event.
// 2. In the keypress event, we map the charcode to the
//    last keydown keycode we received.
// 3. On keyup, we use the keycode to look up the corresponding
//    charcode, and send the charcode on to Pd
var pd_keymap = {}; // to iteratively map keydown/keyup keys
                    // to keypress char codes

function set_keymap(keycode, charcode) {
    pd_keymap[keycode] = charcode;
}

exports.set_keymap = set_keymap;

function get_char_code(keycode) {
    return pd_keymap[keycode];
}

exports.get_char_code = get_char_code;

// This could probably be in pdgui.js
function add_keymods(key, evt) {
    var shift = evt.shiftKey ? "Shift" : "";
    var ctrl = evt.ctrlKey ? "Ctrl" : "";
    return shift + ctrl + key;
}

function cmd_or_ctrl_key(evt) {
    if (process.platform === "darwin") {
        return evt.metaKey;
    } else {
        return evt.ctrlKey;
    }
}

exports.cmd_or_ctrl_key = cmd_or_ctrl_key;

// ico@vt.edu 2022-06-29: added global variable for updating canvas background
var alt_key = 0;

(function () {

    var last_keydown = "";
    var keydown_repeat = 0;

    exports.keydown = function(cid, evt) {
        var key_code = evt.keyCode,
            hack = null, // hack for non-printable ascii codes
            cmd_or_ctrl
        switch(key_code) {
            case 8: // backspace
            case 9:
            case 10:
            case 27:
            //case 32:
            case 127: hack = key_code; break;
            case 46: hack = 127; break; // some platforms report 46 for Delete
            case 37: hack = add_keymods("Left", evt); break;
            case 38: hack = add_keymods("Up", evt); break;
            case 39: hack = add_keymods("Right", evt); break;
            case 40: hack = add_keymods("Down", evt); break;
            case 33: hack = add_keymods("Prior", evt); break;
            case 34: hack = add_keymods("Next", evt); break;
            case 35: hack = add_keymods("End", evt); break;
            case 36: hack = add_keymods("Home", evt); break;

            // These may be different on Safari...
            case 112: hack = add_keymods("F1", evt); break;
            case 113: hack = add_keymods("F2", evt); break;
            case 114: hack = add_keymods("F3", evt); break;
            case 115: hack = add_keymods("F4", evt); break;
            case 116: hack = add_keymods("F5", evt); break;
            case 117: hack = add_keymods("F6", evt); break;
            case 118: hack = add_keymods("F7", evt); break;
            case 119: hack = add_keymods("F8", evt); break;
            case 120: hack = add_keymods("F9", evt); break;
            case 121: hack = add_keymods("F10", evt); break;
            case 122: hack = add_keymods("F11", evt); break;
            case 123: hack = add_keymods("F12", evt); break;

            // Handle weird behavior for clipboard shortcuts
            // Which don't fire a keypress for some odd reason

            case 65:
                if (cmd_or_ctrl_key(evt)) { // ctrl-a
                    // This is handled in the nwjs menu, but we
                    // add a way to toggle the window menubar
                    // the following command should be uncommented...
                    //pdsend(name, "selectall");
                    hack = 0; // not sure what to report here...
                }
                break;
            case 88:
                if (cmd_or_ctrl_key(evt)) { // ctrl-x
                    // This is handled in the nwjs menubar. If we
                    // add a way to toggle the menubar it will be
                    // handled with the "cut" DOM listener, so we
                    // can probably remove this code...
                    //pdsend(name, "cut");
                    hack = 0; // not sure what to report here...
                }
                break;
            case 67:
                if (cmd_or_ctrl_key(evt)) { // ctrl-c
                    // Handled in nwjs menubar (see above)
                    //pdsend(name, "copy");
                    hack = 0; // not sure what to report here...
                }
                break;
            case 86:
                if (cmd_or_ctrl_key(evt)) { // ctrl-v
                    // We also use "cut" and "copy" DOM event handlers
                    // and leave this code in case we need to change
                    // tactics for some reason.
                    //pdsend(name, "paste");
                    hack = 0; // not sure what to report here...
                }
                break;
            case 90:
                if (cmd_or_ctrl_key(evt)) { // ctrl-z undo/redo
                    // We have to catch undo and redo here.
                    // undo and redo have nw.js menu item shortcuts,
                    // and those shortcuts don't behave consistently
                    // across platforms:
                    // Gnu/Linux: key events for the shortcut do not
                    //   propogate down to the DOM
                    // OSX: key events for the shortcut _do_ propogate
                    //   down to the DOM
                    // Windows: not sure...

                    // Solution-- let the menu item shortcuts handle
                    // undo/redo functionality, and do nothing here...
                    //if (evt.shiftKey) {
                    //    pdsend(name, "redo");
                    //} else {
                    //    pdsend(name, "undo");
                    //}
                }
                break;

            // Need to handle Control key, Alt

            case 16: hack = "Shift"; break;
            case 17: hack = "Control"; break;
            case 18:
                hack = "Alt";
                alt_key = 1;
                break;

            // keycode 55 = 7 key (shifted = '/' on German keyboards)
            case 55:
                if (cmd_or_ctrl_key(evt)) {
                    evt.preventDefault();
                    pdsend("pd dsp 1");
                }
                break;

        }
        if (hack !== null) {
            // To match Pd Vanilla behavior, fake a keyup if this
            // is an auto-repeating key
            if (evt.repeat) {
                canvas_sendkey(cid, 0, evt, hack, 1);
            }
            canvas_sendkey(cid, 1, evt, hack, evt.repeat);
            set_keymap(key_code, hack);
        }

        //post("keydown time: keycode is " + evt.keyCode);
        last_keydown = evt.keyCode;
        keydown_repeat = evt.repeat;
        //evt.stopPropagation();
        //evt.preventDefault();
    };

    exports.keypress = function(cid, evt) {
        // For some reasons <ctrl-e> registers a keypress with
        // charCode of 5. We filter that out here so it doesn't
        // cause trouble when toggling editmode.
        // Also, we're capturing <ctrl-or-cmd-Enter> in the "Edit"
        // menu item "reselect", so we filter it out here as well.
        // (That may change once we find a more flexible way of
        // handling keyboard shortcuts
        if (evt.charCode !== 5 &&
              (!cmd_or_ctrl_key(evt) || evt.charCode !== 10)) {
            // To match Pd Vanilla behavior, fake a keyup if this
            // is an auto-repeating key
            if (keydown_repeat) {
                canvas_sendkey(cid, 0, evt, evt.charCode, 1);
            }
            canvas_sendkey(cid, 1, evt, evt.charCode,
                keydown_repeat);
            set_keymap(last_keydown, evt.charCode,
                keydown_repeat);
        }
        //post("keypress time: charcode is " + evt.charCode);
        // Don't do things like scrolling on space, arrow keys, etc.
    };

    exports.keyup = function(cid, evt) {
        var my_char_code = get_char_code(evt.keyCode);
        // Sometimes we don't have char_code. For example, the
        // nw menu doesn't propogate shortcut events, so we don't get
        // to map a charcode on keydown/keypress. In those cases we'll
        // get null, so we check for that here...

        if (my_char_code === "Alt") {
            alt_key = 0;
        }
        //post("keyup char code is " + my_char_code);

        // Also, HTML5 keyup event appears not to ever trigger on autorepeat.
        // So we always send a zero here and fake the autorepeat above to
        // maintain consistency with Pd Vanilla.
        if (my_char_code) {
            canvas_sendkey(cid, 0, evt, my_char_code, 0);
        }
        // This can probably be removed
        //if (cmd_or_ctrl_key(evt) &&
        //      (evt.keyCode === 13 || evt.keyCode === 10)) {
        //    pdgui.pdsend(name, "reselect");
        //}
    };

})();

    // Hard-coded Pd-l2ork font metrics
/*
var font_fixed_metrics = [
    8, 5, 11,
    9, 6, 12,
    10, 6, 13,
    12, 7, 16,
    14, 8, 17,
    16, 10, 19,
    18, 11, 22,
    24, 14, 29,
    30, 18, 37,
    36, 22, 44 ].join(" ");
*/

// Let's try to get some metrics specific to Node-webkit...
// Hard-coded Pd-l2ork font metrics
var font_fixed_metrics = [
    8, 5, 11,
    9, 6, 12,
    10, 6, 13,
    12, 7, 16,
    14, 8, 17,
    16, 10, 19,
    18, 11, 22,
    24, 14, 29,
    30, 18, 37,
    36, 22, 44 ].join(" ");

// Utility Functions

// This is used to escape spaces and other special delimiters in FUDI
// arguments for dialogs. (The reverse function is sys_decodedialog() in the C
// code.)
function encode_for_dialog(s) {
    s = s.replace(/\+/g, "++");
    s = s.replace(/\s/g, "+_");
    s = s.replace(/\$/g, "+d");
    s = s.replace(/;/g, "+s");
    s = s.replace(/,/g, "+c");
    s = "+" + s;
    return s;
}

exports.encode_for_dialog = encode_for_dialog;

// originally used to enquote a string to send it to a tcl function
function enquote (x) {
    var foo = x.replace(/,/g, "");
    foo = foo.replace(/;/g, "");
    foo = foo.replace(/"/g, "");
    foo = foo.replace(/ /g, "\\ ");
    foo = foo.trim();
    return foo;
}

// from stackoverflow.com/questions/21698906/how-to-check-if-a-path-is-absolute-or-relative
// only seems to be used by pddplink_open
function path_is_absolute(myPath) {
    var ret = (path.resolve(myPath) ===
        path.normalize(myPath).replace(/(.+)([\/|\\])$/, "$1"));
    return ret;
}

function set_midiapi(val) {
    pd_whichmidiapi = val;
}

function set_audioapi(val) {
    pd_whichapi = val;
}

var throttle_console_scroll = (function() {
    var scroll_delay;
    return function() {
        if (!scroll_delay) {
            scroll_delay = setTimeout(function() {
                var printout = pd_window.document
                    .getElementById("console_bottom");
                printout.scrollTop = printout.scrollHeight;
                scroll_delay = undefined;
            }, 30);
        }
    }
}());

// Hmm, probably need a closure here...
var current_string = "";
var last_string = "";
var last_child = {};
var last_object_id = "";
var duplicate = 0;

function do_post(object_id, selector, string, type, loglevel) {
    var my_p, my_a, span, text, sel_span, printout, dup_span;
    current_string = current_string + (selector ? selector : "") + string;
    my_p = pd_window.document.getElementById("p1");
    // We can get posts from Pd that are build incrementally, with the final
    // message having a "\n" at the end. So we test for that.
    if (string.slice(-1) === "\n") {
        if (current_string === last_string
            && object_id === last_object_id) {
            duplicate += 1;
            dup_span = last_child.firstElementChild;
            dup_span.textContent = "[" + (duplicate + 1) + "] ";
            current_string = "";
            if (my_p.lastChild !== last_child) {
                my_p.appendChild(last_child);
            }
        } else {
            span = pd_window.document.createElement("span");
            if (type) {
                span.classList.add(type);
            }
            dup_span = pd_window.document.createElement("span");
            sel_span = pd_window.document.createTextNode(
                selector ? selector : "");
            text = pd_window.document.createTextNode(
                (selector && selector !== "") ? ": " + string : current_string);
            if (object_id && object_id.length > 0) {
                my_a = pd_window.document.createElement("a");
                my_a.href = "javascript:pdgui.pd_error_select_by_id('" +
                    object_id + "')";
                my_a.appendChild(sel_span);
                span.appendChild(dup_span); // duplicate tally
                span.appendChild(my_a);
            } else {
                span.appendChild(dup_span);
                span.appendChild(sel_span);
                my_p.appendChild(span);
            }
            span.appendChild(text);
            my_p.appendChild(span);
            last_string = current_string;
            current_string = "";
            last_child = span;
            last_object_id = object_id;
            duplicate = 0;
            // update the scrollbars to the bottom, but throttle it
            // since it is expensive
            throttle_console_scroll();
        }
    }
}

// print message to console-- add a newline for convenience
function post(string, type) {
    do_post(null, null, string + "\n", type, null);
}

exports.post = post;

// print message to console from Pd-- don't add newline
function gui_post(string, type) {
    do_post(null, "", string.replace(/\\/g,''), type, null);
}

function pd_error_select_by_id(objectid) {
    if (objectid !== null) {
        pdsend("pd findinstance " + objectid);
    }
}

exports.pd_error_select_by_id = pd_error_select_by_id;

function gui_post_error(objectid, loglevel, error_msg) {
    do_post(objectid, "error", error_msg, "error", loglevel);
}

// This is used specifically by [print] so that we can receive the full
// message in a single call. This way we can call do_post with a single
// string message and track the object id with the selector.

function gui_print(object_id, selector, array_of_strings) {
    // Unfortunately the instance finder still uses a "." prefix, so we
    // have to add that here
    do_post("." + object_id, selector, array_of_strings.join(" ") + "\n",
        null, null);
}

function gui_legacy_tcl_command(file, line_number, text) {
    // Print legacy tcl commands on the console. These may still be present in
    // some parts of the code (usually externals) which haven't been converted
    // to the new nw.js gui yet. Usually the presence of such commands
    // indicates a bug that needs to be fixed. This information is most useful
    // for developers, so you may want to comment out the following line if
    // you don't want to see them.
    post("legacy tcl command at " + line_number + " of " + file + ": " + text);
}

function clear_console() {
    var container = pd_window.document.getElementById("p1");
    container.textContent = "";
}

exports.clear_console = clear_console;

// convert canvas dimensions to old tcl/tk geometry
// string format. Unfortunately this is exposed (and
// documented) to the user with the "relocate" message
// in both Pd-Extended and Pd-Vanilla.  So we have to
// keep it here for backwards compatibility.
function pd_geo_string(w, h, x, y) {
    return  [w,"x",h,"+",x,"+",y].join("");
}

// ico@vt.edu: we moved this from index.js, so that we can use the versioning
// to also deal with weird offsets between nwjs 0.14/0.24 and 0.46 and upwards
function check_nw_version(version) {
    // aggraef: check that process.versions["nw"] is at least the given version
    // NOTE: We assume that "0.x.y" > "0.x", and just ignore any -beta
    // suffixes if present.
    var nwjs_array = process.versions["nw"].split("-")[0].
        split(".").map(Number);
    var vers_array = version.split("-")[0].
        split(".").map(Number);
    // lexicographic comparison
    for (var i = 0; i < vers_array.length; ++i) {
        if (nwjs_array.length <= i || vers_array[i] > nwjs_array[i])
            return false;
        else if (vers_array[i] < nwjs_array[i])
            return true;
    }
    return vers_array.length <= nwjs_array.length;
}

exports.check_nw_version = check_nw_version;

// ico@vt.edu 2025-04-26: Linux when using 0.6x version of nw.js
// fails to position the window correctly vertically because it is
// unable to account for its DM's title bar. Inside index.js when
// we open the main pd window (see gui_init function), we calculate
// that offset and then store it in the nw_menu_offset global variable
// below. Therefore the value below is only the default that is used
// by Windows and OSX.

var nw_menu_offset = 25;

function set_nw_menu_offset(val) {
    //post("set_nw_menu_offset " + val);
    nw_menu_offset = val;
}

exports.set_nw_menu_offset = set_nw_menu_offset;

function get_nw_menu_offset() {
    return nw_menu_offset;
}

exports.get_nw_menu_offset = get_nw_menu_offset;

// quick hack so that we can paste pd code from clipboard and
// have it affect an empty canvas' geometry
// requires nw.js API
function gui_canvas_change_geometry(cid, w, h, x, y) {
    gui(cid).get_nw_window(function(nw_win) {
        nw_win.width = w;
        // nw_menu_offset is a kludge to account for menubar
        nw_win.height = h + nw_menu_offset;
        nw_win.x = x;
        nw_win.y = y;
    });
}

// In tcl/tk, this function had some checks to apparently
// keep from sending a "relocate" message to Pd, but I'm
// not exactly clear on how it works. If this ends up being
// a cpu hog, check out pdtk_canvas_checkgeometry in the old
// pd.tk
function canvas_check_geometry(cid) {
    //post("canvas_check_geomtery\n...OLD height=" +
    //  patchwin[cid].height + " innerHeight=" + patchwin[cid].window.innerHeight +
    //  " offset=" + nw_menu_offset);
    var win_w = patchwin[cid].width,
        // ico@vt.edu 2025-04-30:
        // to fully appreciate what is going on in here, see how the window sizes are
        // adjusted inside index.js when they are being created, to ensure they have
        // the same size and position when running on different platforms. index.js'
        // nw_create_window and gui_init hold insight into what is going on. more to
        // the point, RPi appears to have 0 nw_menu_offset, which is then compounded
        // by an increase of height by 20 pixels to match that of Windows and other
        // platforms.
        win_h = patchwin[cid].height - (nw_menu_offset === 0 ? 20 : nw_menu_offset),
        win_x = patchwin[cid].x,
        win_y = patchwin[cid].y,
        cnv_width = patchwin[cid].window.innerWidth,
        cnv_height = patchwin[cid].window.innerHeight;
    //post("...NEW height=" +
    //  win_h + " innerHeight=" + cnv_height);
    //post("canvas_check_geometry w=" + win_w + " h=" + win_h +
    //    " x=" + win_x + " y=" + win_y + " cnv_w=" + cnv_width + " cnv_h=" + cnv_height);

    // ico@vt.edu 2025-04-30:
    // to fully appreciate what is going on in here, see how the window sizes are
    // adjusted inside index.js when they are being created, to ensure they have
    // the same size and position when running on different platforms. index.js'
    // nw_create_window and gui_init hold insight into what is going on.
    win_h += (29 * nw_os_is_osx) + (16 * nw_os_is_linux * (nw_menu_offset > 0 ? 1 : 0));
    
    // We're reusing win_x and win_y below, as it
    // shouldn't make a difference to the bounds
    // algorithm in Pd (ico@vt.edu: this is not true anymore for nw 0.46+)
    //post("relocate " + pd_geo_string(cnv_width, cnv_height, win_x, win_y) + " " +
    //       pd_geo_string(cnv_width, cnv_height, win_x, win_y));
    // IMPORTANT! ico@vt.edu: for nw 0.46+ we will need to replace first pd_geo_string's
    // first two args (win_w and win_h with cnv_width and cnv_height + nw_menu_offset
    // to ensure the window reopens exactly how it was saved)
    pdsend(cid, "relocate",
           pd_geo_string(win_w, win_h, win_x, win_y),
           pd_geo_string(cnv_width, cnv_height, win_x, win_y)
    );
}

exports.canvas_check_geometry = canvas_check_geometry;

function menu_save(name) {
    pdsend(name + " menusave");
    populate_open_windows();
}

exports.menu_save = menu_save;

// This is an enormous workaround based off of the comment for this bug:
//   https://github.com/nwjs/nw.js/issues/3372
// Essentially nwworkingdir won't work if you set it through a javascript
// object like a normal human being. Instead, you have to let it get parsed
// by the browser, which means adding a <span> tag as a parent just so we
// can set its innerHTML.
// If this bug is ever resolved then this function can go away, as you should
// be able to just set nwsaveas and nwworkingdir through the setAttribute
// DOM interface.
function build_file_dialog_string(obj) {
    var prop, input = "<input ";
    for (prop in obj) {
        if (obj.hasOwnProperty(prop)) {
            input += prop;
            if (obj[prop]) {
                input += '="' + obj[prop] + '"';
            }
            input += ' ';
        }
    }
    input += "/>";
    return input;
}

exports.build_file_dialog_string = build_file_dialog_string;

function gui_canvas_saveas(name, initfile, initdir, close_flag) {
    //post("gui_canvas_saveas");
    var input, chooser,
        span = patchwin[name].window.document.querySelector("#saveDialogSpan");
    if (!fs.existsSync(initdir)) {
        initdir = pwd;
    }
    // If we don't have a ".pd" file extension (e.g., "Untitled-1", add one)
    if (initfile.slice(-3) !== ".pd") {
        initfile += ".pd";
    }
    // This is complicated because of a bug... see build_file_dialog_string

    // NOTE ag: The original code had nwworkingdir set to path.join(initdir,
    // initfile) which doesn't seem right and in fact does *not* work with the
    // latest nw.js on Linux at all (dialog comes up without a path under
    // which to save, "Save" doesn't work until you explicitly select one).

    // Setting nwsaveas to initfile and nwworkingdir to initdir (as you'd
    // expect) works for me on Linux, but it seems that specifying an absolute
    // pathname for nwsaveas is necessary on Windows, and this also works on
    // Linux. Cf. https://github.com/nwjs/nw.js/issues/3372 (which is still
    // open at the time of this writing). -ag
    input = build_file_dialog_string({
        style: "display: none;",
        type: "file",
        id: "saveDialog",
        // using an absolute path here, see comment above
        nwsaveas: check_nw_version("0.46") ? initfile : path.join(initdir, initfile),
        nwworkingdir: initdir,
        accept: ".pd"
    });
    span.innerHTML = input;
    chooser = patchwin[name].window.document.querySelector("#saveDialog");
    chooser.onchange = function() {
        saveas_callback(name, this.value, close_flag);
        // reset value so that we can open the same file twice
        this.value = null;
        console.log("tried to save something");
    }
    chooser.click();
}

function saveas_callback(cid, file, close_flag) {
    var filename = defunkify_windows_path(file),
        directory = path.dirname(filename),
        basename = path.basename(filename);
    // It probably isn't possible to arrive at the callback with an
    // empty string.  But I've only tested on Debian so far...
    if (filename === null) {
        return;
    }
    pdsend(cid, "savetofile", enquote(basename), enquote(directory),
        close_flag);
    // XXXREVIEW: It seems sensible that we also switch the opendir here. -ag
    set_pd_opendir(directory);
    // update the recent files list
    var norm_path = path.normalize(directory);
    pdsend("pd add-recent-file", enquote(defunkify_windows_path(path.join(norm_path, basename))));
    populate_open_windows();
}

exports.saveas_callback = saveas_callback;

function menu_saveas(name) {
    pdsend(name + " menusaveas");
}

exports.menu_saveas = menu_saveas;

function gui_canvas_print(name, initfile, initdir) {
    // AG: This works mostly like gui_canvas_saveas above, except that we
    // create a pdf file and use a different input element and callback.
    var input, chooser,
        span = patchwin[name].window.document.querySelector("#printDialogSpan");
    if (!fs.existsSync(initdir)) {
        initdir = pwd;
    }
    // If we don't have a ".pd" file extension (e.g., "Untitled-1", add one)
    if (initfile.slice(-3) !== ".pd") {
        initfile += ".pd";
    }
    // Adding an "f" now gives .pdf which is what we want.
    initfile += "f";
    input = build_file_dialog_string({
        style: "display: none;",
        type: "file",
        id: "printDialog",
        nwsaveas: path.join(initdir, initfile),
        nwworkingdir: initdir,
        accept: ".pdf"
    });
    span.innerHTML = input;
    chooser = patchwin[name].window.document.querySelector("#printDialog");
    chooser.onchange = function() {
        print_callback(name, this.value);
        // reset value so that we can open the same file twice
        this.value = null;
        console.log("tried to print something");
    }
    chooser.click();
}

function print_callback(cid, file) {
    var filename = defunkify_windows_path(file);
    // It probably isn't possible to arrive at the callback with an
    // empty string.  But I've only tested on Debian so far...
    if (filename === null) {
        return;
    }
    // Let nw.js do the rest (requires nw.js 0.14.6+)
    patchwin[cid].print({ pdf_path: filename, headerFooterEnabled: false });
    post("printed to: " + filename);
}

exports.print_callback = print_callback;

function menu_print(name) {
    pdsend(name + " menuprint");
}

exports.menu_print = menu_print;

function menu_new () {
    // try not to use a global here
    untitled_directory = get_pd_opendir();
    pdsend("pd filename",
           "Untitled-" + untitled_number,
           enquote(defunkify_windows_path(untitled_directory)));

    // ico@vt.edu 2022-12-05: made invisible preset_hub with
    // k12 scope mandatory for all new patches due to the
    // ability to now change k12 mode at runtime.
    pdsend("#N canvas");
    pdsend("#X obj -30 -30 preset_hub k12 1 %hidden%");
    pdsend("#X pop 1");
    /*} else {
        pdsend("#N canvas");
        pdsend("#X pop 1");
    }*/
    untitled_number++;
}

exports.menu_new = menu_new;

// requires nw.js API
function gui_window_close(cid) {
    // for some edge cases like [vis 1, vis 0(--[send subpatch] we
    // may not have finished creating the window yet. So we check to
    // make sure the canvas cid exists...
    gui(cid).get_nw_window(function(nw_win) {
        nw_close_window(nw_win);
    });
    // remove reference to the window from patchwin object
    set_patchwin(cid, null);
    populate_open_windows();
    loading[cid] = null;
    editable[cid] = null;
}

function menu_k12_open_demos(cid) {
    //post("menu_k12_open_demos " +
    //    defunkify_windows_path(lib_dir + "/extra/K12/demos"));
    gui(cid).get_nw_window(function(nw_win) {
        //post("...found window");
        var input, chooser,
        span = nw_win.window.document.querySelector("#fileDialogSpan");
        // Complicated workaround-- see comment in build_file_dialog_string
        input = build_file_dialog_string({
            style: "display: none;",
            type: "file",
            id: "fileDialog",
            nwworkingdir: (nw_os_is_windows ? 
                funkify_windows_path(lib_dir + "/extra/K12/demos") :
                (lib_dir + "/extra/K12/demos")),
            multiple: null,
            // These are copied from pd_filetypes in pdgui.js
            accept: ".pd,.pat,.mxt,.mxb,.help"
        });
        span.innerHTML = input;
        chooser = nw_win.window.document.querySelector("#fileDialog");
        // Hack-- we have to set the event listener here because we
        // changed out the innerHTML above
        chooser.onchange = function() {
            var file_array = this.value;
            // reset value so that we can open the same file twice
            this.value = null;
            menu_open(file_array);
            //console.log("tried to open the k12 demos folder");
        };
        chooser.click();
    });
}

exports.menu_k12_open_demos = menu_k12_open_demos;

// LATER: rethink how files are separated since
// semicolon can be embedded inside a filename
// this will require rethinking menu_open globally
function menu_open (filenames_string) {
    var file_array = filenames_string.split(";"),
        length = file_array.length,
        i;
    for (i = 0; i < length; i++) {
        open_file(file_array[i]);
    }
}

exports.menu_open = menu_open;

function menu_close(name) {
    // not handling the "text editor" yet
    // not handling the "Window" menu yet
    //pdtk_canvas_checkgeometry $name
    pdsend(name + " menuclose 0");
}

exports.menu_close = menu_close;

function canvas_menu_set_editable(cid, val) {
    //post("canvas_menu_set_editable cid=" + cid + " val=" + val);
    editable[cid] = val;
    // update menu if this window is visible
    gui(cid).get_nw_window(function(nw_win) {
        //post("...found window cid=" + cid);
        nw_win.window.set_menu_modals(cid, val);
    });
}

exports.canvas_menu_set_editable = canvas_menu_set_editable;

function canvas_menu_get_editable(cid) {
    if (editable[cid] != null) {
        return editable[cid];
    } else {
        return true;
    }
}

exports.canvas_menu_get_editable = canvas_menu_get_editable;

function canvas_menuclose_callback(cid_for_dialog, cid, force) {
    // Hacky-- this should really be dir/filename here instead of
    // filename/args/dir which is ugly. Also, this should use the
    // html5 dialog-- or some CSS equivalent-- instead of the
    // confusing OK/Cancel javascript prompt.
    var nw = patchwin[cid_for_dialog],
        w = nw.window,
        doc = w.document,
        dialog = doc.getElementById("save_before_quit"),
        // hm... we messed up somewhere. It'd be better to set the document
        // title so that we don't have to mess with nw.js-specific properties.
        // Also, it's pretty shoddy to have to split on " * ", and to include
        // the creation arguments in this dialog. We'd be better off storing
        // the actual path and filename somewhere, then just fetching it here.
        title = nw.title.split(" * "),
        dialog_file_slot = doc.getElementById("save_before_quit_filename"),
        yes_button = doc.getElementById("yes_button"),
        no_button = doc.getElementById("no_button"),
        cancel_button = doc.getElementById("cancel_button"),
        filename = title[0],
        dir = title[1];
    if (dir.charAt(0) === "(") {
        dir = dir.slice(dir.indexOf(")")+4); // slice off ") - "
    } else {
        dir = dir.slice(2); // slice off "- "
    }
    dialog_file_slot.textContent = filename;
    dialog_file_slot.title = dir;
    yes_button.onclick = function() {
        w.canvas_events.save_and_close();
    };
    no_button.onclick = function() {
        w.canvas_events.close_without_saving(cid, force);
    };
    cancel_button.onclick = function() {
        w.canvas_events.close_save_dialog();
        w.canvas_events[w.canvas_events.get_previous_state()]();
    }

    // Boy does this seem wrong-- restore() brings the window to the front of
    // the stacking order. But that is really the job of focus(). This works
    // under Ubuntu-- need to test it on OSX...
    nw.restore();
    // Turn off events so that the user doesn't change the canvas state--
    // we actually need to disable the menubar items, too, but we haven't
    // done that yet.
    w.canvas_events.none();
    // catch escape and properly restore canvas events, otherwise the
    // default escape commands closes the dialog but does not restore
    // them, leaving the patch in an inoperable state
    doc.onkeydown = function(evt) {
        //post("onkeydown=" + evt.keyCode + " ctrl=" +
        //    evt.ctrlKey + " meta=" + evt.metaKey);
        if (evt.keyCode === 27) { // escape
            w.canvas_events.close_save_dialog();
            w.canvas_events[w.canvas_events.get_previous_state()]();
        }
    };
    // go back to original zoom level so that dialog will show up
    // this is no longer necessary
    //nw.zoomLevel = 0;
    // ico 2023-10-23: here we cannot reset the scale since it is a
    // read-only variable
    //post("resetting scale " + w.visualViewport.scale);
    //w.visualViewport.scale = 1;
    // big workaround-- apparently the dialog placement algo and the nw.js
    // zoomLevel state change don't happen deterministically. So we set a
    // timeout to force the dialog to render after the zoomLevel change.
    // ico@vt.edu 2022-12-15: update K12_menu and getscroll to update
    // scrollbars in case we are changing zoom level. otherwise menu and
    // scrollbars end-up being incorrect. we delay this to allow for
    // zoom to update.
    //post("canvas_menuclose_callback");
    w.setTimeout(function() {
        update_k12_menu(cid);
        do_getscroll(cid, 1);
    }, 0);

    // Probably the best solution is to give up on using the nw.js zoomLevel
    // method altogether and do canvas zooming completely in the DOM. This will
    // add some math to the canvas_events, so it's probably a good idea to
    // wait until we move most of the GUI functionality out of the C code (or
    // at least until we quit sending incessant "motion" messages to the core).
    w.setTimeout(function() {
        dialog.showModal();
    }, 150);
}

function gui_canvas_menuclose(cid_for_dialog, cid, force) {
    // Hack to get around a renderer bug-- not guaranteed to work
    // for long patches
    setTimeout(function() {
            canvas_menuclose_callback(cid_for_dialog, cid, force);
        }, 450);
}

function canvas_abstract_callback(cid_for_dialog, cid, matches) {
    var nw = patchwin[cid_for_dialog],
        w = nw.window,
        doc = w.document,
        dialog = doc.getElementById("abstract_dialog"),
        dialog_candidates = doc.getElementById("abstract_dialog_candidates"),
        single_button = doc.getElementById("abstract_single_button"),
        all_button = doc.getElementById("abstract_all_button"),
        none_button = doc.getElementById("abstract_none_button");

    dialog_candidates.textContent = matches.toString();
    dialog_candidates.title = matches.toString()
    all_button.disabled = (matches === 1);

    single_button.onclick = function() {
        dialog.close();
        w.canvas_events[w.canvas_events.get_previous_state()]();
        pdsend(cid, "dialog", 0);
    };
    all_button.onclick = function() {
        dialog.close();
        w.canvas_events[w.canvas_events.get_previous_state()]();
        pdsend(cid, "dialog", 1);
    };
    none_button.onclick = function() {
        dialog.close();
        w.canvas_events[w.canvas_events.get_previous_state()]();
    }

    w.canvas_events.none();

    w.setTimeout(function() {
        dialog.showModal();
    }, 150);
}

function gui_canvas_abstract(cid_for_dialog, cid, matches) {
    setTimeout(function() {
            canvas_abstract_callback(cid_for_dialog, cid, matches);
        }, 450);
}

function gui_quit_dialog() {
    gui_raise_pd_window();
    var reply = pd_window.window.confirm("Really quit?");
    if (reply === true) {
        pdsend("pd quit");
    }
}

// send a message to Pd or nwjs (by prepending the message with "nwjs:)
function menu_send(name) {
    var message,
        win = name ? patchwin[name] : pd_window;
    message = win.window.prompt("Type a message to send to Pd-L2Ork", name);
    if (message != undefined && message.length) {
        var semi = (message.endsWith(";") ? "" : ";");
        if (message.startsWith("nwjs:")) {
            post("Sending message to nwjs: " + message.replace("nwjs:", "") + semi);
            eval(message.replace("nwjs:", "") + ";");
        } else {
            post("Sending message to Pd: " + message + semi);
            pdsend(message);
        }
    }
}

/*
ico@vt.edu 20200907: added svg tiled background to reflect edit mode and
integrated it into the canvas_set_editmode below.
20220627: Merged with further improvements from Purr-Data that allow for
snap to grid.
*/

// Set the grid background position to adjust for the viewBox of the svg.
// We do this separately and before setting the background so we can call this
// when the scroll view needs to be adjusted.
function get_grid_coords(cid, svg_elem) {
    var vbox = svg_elem.getAttribute("viewBox").split(" "),
        dx = 0, dy = 0;
    // First two values of viewBox are x-origin and y-origin. Pd allows
    // negative coordinates-- for example, the user can drag an object at
    // (0, 0) 12 pixels to the left to arrive at (-12, 0). To accommodate this
    // with the svg backend, we would adjust the x-origin to be -12 so that
    // the user can view it (possibly by scrolling). These adjustments are
    // all handled with gui_canvas_get_scroll.
    //
    // For the background image css property, everything is based on
    // CSS DOM positioning. CSS doesn't really know anything about the SVG
    // viewport-- it only knows that an SVG element is of a certain size and
    // (in our case) has its top-left corner at the top-left corner of the
    // window. So when we change the viewBox to have negative origin indices,
    // we have to adjust the origin of the grid in the opposite direction
    // For example, if our new x-origin for the svg viewBox is -12, we make
    // the x-origin for the background image "12px". This adjustment positions
    // the grid *as if* if extended 12 more pixels to the left of its
    // container.
    if (vbox[0] < 0) {
        dx = 0 - vbox[0];
    }
    if (vbox[1] < 0) {
        dy = 0 - vbox[1];
    }
    return { x: dx, y: dy };
}

// 2022-06-29 ico@vt.edu:
// currently unused as this is created behind other objects and therefore
// potentially useless
function create_svg_lock(cid) {
    var zoom = patchwin[cid].zoomLevel,
        size;
    // adjust for zoom level
    size = 1 / Math.pow(1.2, zoom) * 24;
    return "url('data:image/svg+xml;utf8," +
        encodeURIComponent(['<svg xmlns="http://www.w3.org/2000/svg"',
                 ['width="', size, 'px"'].join(""),
                 ['height="', size, 'px"'].join(""),
                 'viewBox="0 0 486.866 486.866"',
            '>',
              '<path fill="#bbb" d="',
                'M393.904,214.852h-8.891v-72.198c0-76.962-61.075-141.253',
                '-137.411-142.625c-2.084-0.038-6.254-0.038-8.338,0',
                'C162.927,1.4,101.853,65.691,101.853,142.653v1.603c0,16.182,',
                '13.118,29.3,29.3,29.3c16.182,0,29.299-13.118,29.299-29.3',
                'v-1.603',
                'c0-45.845,37.257-83.752,82.98-83.752s82.981,37.907,82.981,',
                '83.752v72.198H92.963c-13.702,0-24.878,14.139-24.878,',
                '31.602v208.701',
                'c0,17.44,11.176,31.712,24.878,31.712h300.941c13.703,0,',
                '24.878-14.271,24.878-31.712V246.452',
                'C418.783,228.989,407.607,214.852,393.904,214.852z M271.627,',
                '350.591v63.062c0,7.222-6.046,13.332-13.273,13.332h-29.841',
                'c-7.228,0-13.273-6.11-13.273-13.332v-63.062c-7.009-6.9-11.09',
                '-16.44-11.09-26.993c0-19.999,15.459-37.185,35.115-37.977',
                'c2.083-0.085,6.255-0.085,8.337,0c19.656,0.792,35.115,17.978,',
                '35.115,37.977C282.717,334.149,278.637,343.69,271.627,350.591z',
              '"/>',
            '</svg>',
        "')",
    ].join(" "));
}

// background image color helper function
function get_css_svg_color(cid, which) {
    var getvar = 0;
    switch(which) {
        case 0: getvar = '--bg_large_grid'; break;
        case 1: getvar = '--bg_small_grid'; break;
        case 2: getvar = '--default_array_fill'; break;
        case 3: getvar = '--default_array_outline'; break;
    }
    var ret = getComputedStyle(patchwin[cid].window.document.documentElement).
                getPropertyValue(getvar);
    //post("get_css_svg_color which=" + which + " value=" + ret);
    return ret;
}

// Background for edit mode. Currently, we use a grid if snap-to-grid
// functionality is turned on in the GUI preferences. If not, we just use
// the same grid with a lower opacity. That way the edit mode is always
// visually distinct from run mode.
var create_editmode_bg = function(cid, svg_elem) {
    var data, cell_data_str, opacity_str, grid, size, pos;
    grid = showgrid[cid];
    size = gridsize[cid];
    pos = get_grid_coords(cid, svg_elem);
    // if snap-to-grid isn't turned on, just use cell size of 10 and make the
    // grid partially transparent
    size = grid ? size : 10;
    opacity_str = '"' + (grid ? 1 : 0.4) + '"';
    cell_data_str = ['"', "M", size, 0, "L", 0, 0, 0, size, '"'].join(" ");

    data = ['<svg xmlns="http://www.w3.org/2000/svg" ',
                'width="1000" height="1000" ',
                'opacity=', opacity_str, '>',
              '<defs>',
                '<pattern id="cell" patternUnits="userSpaceOnUse" ',
                         'width="', size, '" height="', size, '">',
                  '<path fill="none" stroke="', get_css_svg_color(cid, 1),
                        '" stroke-width="1" d=', cell_data_str,'/>',
                '</pattern>',
                '<pattern id="grid" patternUnits="userSpaceOnUse" ',
                    'width="100" height="100" x="', pos.x, '" y="', pos.y, '">',
                  '<rect width="500" height="500" fill="url(#cell)" />',
                  '<path fill="none" stroke="', get_css_svg_color(cid, 0),
                        '" stroke-width="1" d="M 500 0 L 0 0 0 500"/>',
                '</pattern>',
              '</defs>',
              '<rect width="1000" height="1000" fill="url(#grid)" />',
            '</svg>'
        ].join(" ");
    // make sure to encode the data so we obey all the rules with our data URL
    return "url('data:image/svg+xml;utf8," + encodeURIComponent(data) + "')";
}

var create_editmode_key_runmode_bg = function(cid, svg_elem) {
    var data, cell_data_str, opacity_str, grid, size, pos;
    grid = showgrid[cid];
    size = gridsize[cid];
    pos = get_grid_coords(cid, svg_elem);
    // if snap-to-grid isn't turned on, just use cell size of 10 and make the
    // grid partially transparent
    size = grid ? size : 10;
    opacity_str = '"' + (grid ? 1 : 0.4) + '"';
    cell_data_str = ['"', "M", size, 0, "L", 0, 0, 0, size, '"'].join(" ");

    data = ['<svg xmlns="http://www.w3.org/2000/svg" ',
                'width="1000" height="1000" ',
                'opacity=', opacity_str, '>',
              '<defs>',
                '<pattern id="grid" patternUnits="userSpaceOnUse" ',
                    'width="100" height="100" x="', pos.x, '" y="', pos.y, '">',
                  '<rect width="500" height="500" fill="url(#cell)" />',
                  '<path fill="none" stroke="', get_css_svg_color(cid, 0),
                        '" stroke-width="1" d="M 500 0 L 0 0 0 500"/>',
                '</pattern>',
              '</defs>',
              '<rect width="1000" height="1000" fill="url(#grid)" />',
            '</svg>'
        ].join(" ");
    // make sure to encode the data so we obey all the rules with our data URL
    return "url('data:image/svg+xml;utf8," + encodeURIComponent(data) + "')";
}

function set_bg(cid, data_url, bg_pos, repeat) {
    var style = patchwin[cid].window.document.body.style;
    style.setProperty("background-image", data_url);
    style.setProperty("background-position", bg_pos);
    style.setProperty("background-repeat", repeat);
}

function set_editmode_bg(cid, svg_elem, state)
{
    //post("set_editmode_bg " + cid + " state=" + state + " alt=" + alt_key);
    var offset, zoom;
    if (!state) {
        if (alt_key)
            //set_bg(cid, create_editmode_key_runmode_bg(cid, svg_elem),
            //    "0% 0%", "repeat");
            // ico@bukvic.net 2025-10-29:
            // this is too messy due to alt key autorepeat, so we are disabling
            // different look for the alt key pressed temporary runtime. it is
            // only cosmetic anyhow.
            set_bg(cid, "none", "0% 0%", "repeat");
        else
            set_bg(cid, "none", "0% 0%", "repeat");
    } else {
        if (alt_key)
            //set_bg(cid, create_editmode_key_runmode_bg(cid, svg_elem),
            //    "0% 0%", "repeat");
            set_bg(cid, "none", "0% 0%", "repeat");
        else
            set_bg(cid, create_editmode_bg(cid, svg_elem), "0% 0%", "repeat");
    }
}

function update_svg_background(cid, svg_elem) {
    //post("update_svg_background " + cid);
    var bg = patchwin[cid].window.document.body.style
        .getPropertyValue("background-image");
    // Quick hack-- we just check whether the background has been drawn. If
    // it has we assume we're in editmode.
    //post("...bg=" + bg);
    if (bg !== "none") {
        set_editmode_bg(cid, svg_elem, true);
    }
}

function canvas_fake_alt_key_release(cid) {
    //post("fake_alt_key_release");
    alt_key = 0;
}

exports.canvas_fake_alt_key_release = canvas_fake_alt_key_release;

function gobj_set_runtime_tooltip(cid, tag, tooltip) {
    //post("gobj_set_runtime_tooltip cid=" + cid + " tag=" + tag);
    var elem;
    gui(cid).get_elem(tag + "gobj", function(e) {
        elem = e;
        e.setAttribute("runtime_tooltip", tooltip.replace(/\\/g,''));
    });

    gui(cid).get_elem("patchsvg", function(patchsvg) {
        var editmode = patchsvg.classList.contains("editmode");
        if (!editmode) {
            elem.getElementsByTagName('title')[0].textContent =
                elem.getAttribute('runtime_tooltip');
        }
    });
}

exports.gobj_set_runtime_tooltip = gobj_set_runtime_tooltip;

// requires nw.js API (Menuitem)
// this is called from nwjs
function canvas_set_editmode(cid, state) {
    //post("canvas_set_editmode " + cid + " " + state);
    gui(cid).get_elem("patchsvg", function(patchsvg, w) {
        w.set_editmode_checkbox(state !== 0 ? true : false);
        //post("...state=" + state);
        if (state !== 0) {
            patchsvg.classList.add("editmode");
            // For now, we just change the opacity of the background grid
            // depending on whether snap-to-grid is turned on. This way
            // edit mode is always visually distinct.
            set_editmode_bg(cid, patchsvg, true);
            var all_svg_g = patchsvg.getElementsByTagName('g');
            //post("..." + all_svg_g.length);
            for (var i = 0; i < all_svg_g.length; i++) {
                //post("...g=" + all_svg_g[i].getAttribute('id'));
                var tooltip = "";
                if (all_svg_g[i].getAttribute('tooltip'))
                    tooltip = all_svg_g[i].getAttribute('tooltip');
                //post("...tooltip=<" + tooltip + ">");
                if (all_svg_g[i].getElementsByTagName('title').length > 0) {
                    // ico 2023-11-04: we need the frist one since the subpatch
                    // or an abstraction only has one tooltip that applies to
                    // the entire object
                    //var last = all_svg_g[i].getElementsByTagName('title').length - 1;
                    all_svg_g[i].getElementsByTagName('title')[0].textContent = tooltip;
                }
            }
        } else {
            patchsvg.classList.remove("editmode");
            set_editmode_bg(cid, patchsvg, false);
            var all_svg_g = patchsvg.getElementsByTagName('g');
            //post("..." + all_svg_g.length);
            for (var i = 0; i < all_svg_g.length; i++) {
                //post("...g=" + all_svg_g[i].getAttribute('id'));
                var tooltip = "";
                if (all_svg_g[i].getAttribute('runtime_tooltip'))
                    tooltip = all_svg_g[i].getAttribute('runtime_tooltip');
                //post("...tooltip=<" + tooltip + "> length=" +
                //    all_svg_g[i].getElementsByTagName('title').length + " text=<" +
                //    all_svg_g[i].getElementsByTagName('title').innerHTML + ">");
                if (all_svg_g[i].getElementsByTagName('title').length > 0) {
                    for (var j = 0; j < all_svg_g[i].getElementsByTagName('title').length; j++) {
                        //post("..." + j + ":current tooltip=<" +
                        //    all_svg_g[i].getElementsByTagName('title')[j].textContent + ">");
                    }
                    // ico 2023-11-04: we need the frist one since the subpatch
                    // or an abstraction only has one tooltip that applies to
                    // the entire object
                    //var last = all_svg_g[i].getElementsByTagName('title').length - 1;
                    all_svg_g[i].getElementsByTagName('title')[0].textContent = tooltip;
                }
            }
        }
    });
}

exports.canvas_set_editmode = canvas_set_editmode;

// this is called exclusively from c
function gui_canvas_set_editmode(cid, state) {
    canvas_set_editmode(cid, state);
    //post("gui_canvas_set_editmode " + state);
    if (patchwin[cid] !== null) {
        var k12_menu = patchwin[cid].window.document.
            getElementById("k12_menu");
        if (k12_menu.style.display == "block") {
            toggle_k12_menu(cid, state);
        }
    }
}

// ask the engine about the current edit mode
function canvas_query_editmode(cid) {
    pdsend(cid, "query-editmode");
}

exports.canvas_query_editmode = canvas_query_editmode;

function update_grid(grid, grid_size_value) {
    //post("update_grid");
    // Update the grid background of all canvas windows when the corresponding
    // option in the gui prefs changes.
    //var bg = grid != 0 ? gui_editmode_svg_background : "none";
    for (var cid in patchwin) {
        showgrid[cid] = grid !== 0;
        gridsize[cid] = grid_size_value;
    	gui(cid).get_elem("patchsvg", function(patchsvg, w) {
            var editmode = patchsvg.classList.contains("editmode");
            if (editmode) {
                //patchwin[cid].window.document.body.style.setProperty
                //("background-image", bg);
                set_editmode_bg(cid, patchsvg, true);
            }
    	});
    }
    // Also update the showgrid flags.
    //set_showgrid(grid);
}

exports.update_grid = update_grid;

function redraw_all_visible_canvases() {
    // redraw all canvases to reflect the preference setting
    for (var cid in patchwin) {
        if (gui(cid) && gui(cid).get_elem("patchsvg"))
        {
            pdsend(cid, "redraw");
        }
    }
}

exports.redraw_all_visible_canvases = redraw_all_visible_canvases;

// requires nw.js API (Menuitem)
function gui_canvas_set_cordinspector(cid, state) {
    patchwin[cid].window.set_cord_inspector_checkbox(state !== 0 ? true : false);
}

function canvas_set_scrollbars(cid, scroll) {
    //post("canvas_set_scrollbars " + scroll);
    patchwin[cid].window.document.body.style.
        overflow = scroll ? "visible" : "hidden";
    gui_update_scrollbars(cid);

}

exports.canvas_set_scrollbars = canvas_set_scrollbars;

function gui_canvas_hide_scrollbars(cid, no_scrollbars) {
    canvas_set_scrollbars(cid, no_scrollbars === 0);
}

exports.menu_send = menu_send;

function gui_set_toplevel_window_list(dummy, attr_array) {
    // We receive an array in the form:
    // ["Name", "address", etc.]
    // where "address" is the cid (x123456etc.)
    // We don't do anything with it at the moment,
    // but they could be added to the "Windows" menu
    // if desired. (Pd Vanilla doesn't do this, but
    // Pd-l2ork (and possibly Pd-extended) did.

    // the "dummy" parameter is just to work around a bug in the gui_vmess API
}

function menu_quit() {
    pdsend("pd verifyquit");
}

exports.menu_quit = menu_quit;

var nw_app_quit;

function app_quit() {
    nw_app_quit();
}

exports.set_app_quitfn = function(quitfn) {
    nw_app_quit = quitfn;
}

function import_file(directory, basename)
{
    if (basename.match(/\.(pat|mxb|help)$/) !=null) {
        post("warning: opening pat|mxb|help not implemented yet");
        if (pd_nt == 0) {
            // on GNU/Linux, cyclist is installed into /usr/bin usually
            cyclist = "/usr/bin/cyclist";
        } else {
            cyclist = pd_guidir + "/bin/cyclist"
        }
        //The following is from tcl and still needs to be ported...

        //convert Max binary to text .pat
        // The following is tcl code which needs to get converted
        // to javascript...
        //set binport [open "| \"$cyclist\" \"$filename\""]
        //set convertedtext [read $binport]
        //if { ! [catch {close $binport} err]} {
        //    if {! [file writable $directory]} {     set directory "/tmp" }
        //    set basename "$basename.pat"
        //    set textpatfile [open "$directory/$basename" w]
        //    puts $textpatfile $convertedtext
        //    close $textpatfile
        //    puts stderr "converted Max binary to text format: $directory/$basename"
        //}
    }
}

function process_file(file, do_open) {
    //post("process_file " + file + " " + do_open);
    var filename = defunkify_windows_path(file),
        directory = path.dirname(filename),
        basename = path.basename(filename),
        cyclist;
    if (do_open) import_file(directory, basename);
    if (basename.match(/\.(pd|pat|mxt)$/i) != null) {
        if (do_open) {
            pdsend("pd open", enquote(basename),
                   (enquote(directory)));
        }
        set_pd_opendir(directory);
        //::pd_guiprefs::update_recentfiles "$filename" 1
        // update the recent files list
        var norm_path = path.normalize(directory);
        pdsend("pd add-recent-file", enquote(defunkify_windows_path(path.join(norm_path, basename))));
    }
}

function open_file(file) {
    //post("open_file " + file);
    process_file(file, 1);
}

function gui_process_open_arg(file) {
    // AG: This is invoked when the engine opens a patch file via the command
    // line (-open). In this case the file is already loaded, so we just
    // update the opendir and the recent files list.
    process_file(file, 0);
}

function open_html(target) {
    nw_open_html(target);
}

function open_textfile(target) {
    nw_open_textfile(target);
}

// Think about renaming this and pd_doc_open...

// Open a file-- html, text, or Pd.
function doc_open (dir, basename, f) {
    // normalize to get rid of extra slashes, ".." and "."
    var norm_path = path.normalize(dir);
    if (basename.slice(-4) === ".txt"
        || basename.slice(-2) === ".c") {
        open_textfile(path.join(norm_path, basename));
    } else if (basename.slice(-5) === ".html"
               || basename.slice(-4) === ".htm"
               || basename.slice(-4) === ".pdf") {
        open_html(path.join(norm_path, basename));

    } else {
        pdsend("pd open", enquote(defunkify_windows_path(basename)),
            enquote(defunkify_windows_path(norm_path)), f?f:0);
        pdsend("pd add-recent-file",
            enquote(defunkify_windows_path(path.join(norm_path, basename))));
    }
}

// Need to rethink these names-- it's confusing to have this and
// pd_doc_open available, but we need this one for dialog_search because
// it uses absolute paths
exports.doc_open = doc_open;

// Open a file relative to the main directory where "doc/" and "extra/" live
function pd_doc_open(dir, basename, f) {
    doc_open(path.join(lib_dir, dir), basename, f);
}

exports.pd_doc_open = pd_doc_open;

function external_doc_open(url) {
    nw_open_external_doc(url);
}

exports.external_doc_open = external_doc_open;

function gui_set_cwd(dummy, cwd) {
    if (cwd !== ".") {
        pwd = cwd;
        post("working directory is " + cwd);
    }
}

// This doesn't work at the moment. Not sure how to feed the command line
// filelist to a single instance of node-webkit.
function gui_open_via_unique (secondary_pd_engine_id, unique, file_array) {
    //post("gui_open_via_unique " + pd_engine_id + " " + secondary_pd_engine_id + " " + unique + " " + file_array + " os_is_windows=" + nw_os_is_windows);
    var startup_dir = pwd,
        i,
        file;
    if (unique == 0 && (nw_os_is_windows || secondary_pd_engine_id !== pd_engine_id)) {
        for (i = 0; i < file_array.length; i++) {
            file = file_array[i];
            if (!path.isAbsolute(file)) {
                file = path.join(pwd, file);
            }
            open_file(file);
        }
        quit_secondary_pd_instance(secondary_pd_engine_id);
    }
}

function gui_startup(version, fontname_from_pd, fontweight_from_pd,
    apilist, midiapilist) {
    console.log("Starting up...");
    console.log("gui_startup from GUI...");
    // # tb: user defined typefaces
    // set some global variables
    pd_myversion = version;
    pd_apilist =  apilist;
    pd_midiapilist = midiapilist;

    fontname = fontname_from_pd;
    fontweight = fontweight_from_pd;
    pd_fontlist = "";
    untitled_number = 1; // global variable to increment for each new patch

    // From tcl, not sure if all of it is still needed...

    // # on Mac OS X, lower the Pd window to the background so patches open on top
    // if {$pd_nt == 2} { lower . }
    // # on Windows, raise the Pd window so that it has focused when launched
    // if {$pd_nt == 1} { raise . }

    // set fontlist ""
    // if {[info tclversion] >= 8.5} {find_default_font}
    //        set_base_font $fontname_from_pd $fontweight_from_pd
    //        fit_font_into_metrics

    //    # UBUNTU MONO 6 6 8 10 11 14 14 19 22 30
    //        # DEJAVU SANS MONO 6 6 8 9 10 12 14 18 22 29

    //#    foreach i {6 6 8 10 11 14 14 19 22 30} {
    //#        set font [format {{%s} %d %s} $fontname_from_pd $i $fontweight_from_pd]
    //#        set pd_fontlist [linsert $pd_fontlist 100000 $font]
    //#        set width0 [font measure  $font x]
    //#        set height0 [lindex [font metrics $font] 5]
    //#        set fontlist [concat $fontlist $i [font measure  $font x] \
    //#                          [lindex [font metrics $font] 5]]
    //#    }

    //    set tclpatch [info patchlevel]
    //    if {$tclpatch == "8.3.0" || \
    //            $tclpatch == "8.3.1" || \
    //            $tclpatch == "8.3.2" || \
    //            $tclpatch == "8.3.3" } {
    //        set oldtclversion 1
    //    } else {
    //        set oldtclversion 0
    //    }
    pdsend("pd init", enquote(defunkify_windows_path(pwd)), "0",
        font_fixed_metrics);

    //    # add the audio and help menus to the Pd window.  We delayed this
    //    # so that we'd know the value of "apilist".
    //    menu_addstd .mbar

    //    global pd_nt
    //    if {$pd_nt == 2} {
    //        global pd_macdropped pd_macready
    //        set pd_macready 1
    //        foreach file $pd_macdropped {
    //            pd [concat pd open [pdtk_enquote [file tail $file]] \
    //                    [pdtk_enquote  [file dirname $file]] \;]
    //            menu_doc_open [file dirname $file] [file tail $file]
    //        }
    //    }
}

// Global canvas associative arrays (aka javascript objects)
var scroll = {},
    menu = {},
    canvas_color = {},
    topmost = {},
    resize = {},
    xscrollable = {},
    yscrollable = {},
    update_tick = {},
    drag_tick = {},
    undo = {},
    redo = {},
    font = {},
    doscroll = {},
    showgrid = {},
    gridsize = {},
    last_loaded, // last loaded canvas
    last_focused, // last focused canvas (doesn't include Pd window or dialogs)
    loading = {},
    title_queue= {}, // ugly kluge to work around an ugly race condition
    popup_menu = {},
    toplevel_scalars = {},
    editable = {}, // for allowing developer to lock the patch (e.g.) to beginners
    k12_menu_canvas_x = {}; // per patch k12_menu offset when the menu is unfolded

    var patchwin = {}; // object filled with cid: [Window object] pairs
    var dialogwin = {}; // object filled with did: [Window object] pairs

/*var set_showgrid = function(grid) {
    for (var cid in showgrid) {
	showgrid[cid] = grid;
    }
}*/

exports.get_patchwin = function(name) {
    return patchwin[name];
}

var set_patchwin = function(cid, win) {
    //post("set_patchwin cid=" + cid + " win=" + win);
    patchwin[cid] = win;
    if (win) {
        gui.add(cid, win);
    } else {
        gui.remove(cid, win);
    }
}

exports.set_patchwin = set_patchwin;

function get_dialogwin(name) {
    return dialogwin[name];
}

exports.get_dialogwin = get_dialogwin;

exports.set_dialogwin = function(did, win) {
    dialogwin[did] = win;
}

exports.remove_dialogwin = function(name) {
    dialogwin[name] = null;
}

// ico 2023-08-20: this is called by all dialog windows
// to check if the dialog is spilling outside the desktop 
// and adjust its position accordingly.
function dialogwin_check_overflow(win, object) {
    var sw = window.screen.width;
    var sh = window.screen.height;

    // ways to obtain window and screen info
    //post(sw + "," + sh);
    //post(win.height + " " + win.x);

    // deal with bottom and right overflow
    var height = 0;
    switch(object) {
        case "bng":
        case "tgl":
        case "vradio":
        case "hradio":
            height = 490;
            break;
        case "hsl":
        case "vsl":
            height = 547;
            break;
        case "nbx":
            height = 604;
            break;
        case "cnv":
            height = 432;
            break;
        case "vu":
            height = 391;
            break;
        case "ggee/image":
            height = 651;
            break;
        case "Dropdown":
            height = 306;
            break;
        case "Atom":
            height = 373;
            break;
        case "SymAtom":
            height = 305;
            break;
        case "Data":
            height = 345;
            break;
        case "Canvas":
            height = 417;
            break;
        case "Array":
            height = 321;
            break;
        case "Canvas&Array":
            height = 560;
            break;
        default:
            height = 604;
            break;
    }
    /*
    // ico 2023-08-20:
    // we will leave this disabled for the time being
    // as on Windows not all dialogs have a consistent
    // size in comparison to Linux and OSX, so they
    // all might as well be a bit off the bottom,
    // particularly since there can be also a bar at the
    // bottom that is currently not factored in into the
    // screen resolution calculation.
    if (nw_os_is_windows) {
        height -= 6;
    }
    */
    //post("height=" + height);
    if (win.x + win.width > sw)
        win.x -= win.x + win.width - sw;
    if (win.y + height > sh)
        win.y -= win.y + height - sh;
}

exports.dialogwin_check_overflow = dialogwin_check_overflow;

exports.get_toplevel_scalars = function(name) {
    return toplevel_scalars[name];
}

// stopgap...
pd_colors["canvas_color"] = "white";

exports.last_loaded = function () {
    return last_loaded;
}

// close a canvas window

function gui_canvas_cursor(cid, pd_event_type) {
    //post("gui_canvas_cursor " + cid + " " + pd_event_type);
    gui(cid).get_elem("patchsvg", function(patch) {
        // A quick mapping of events to pointers-- these can
        // be revised later
        var c;
        switch(pd_event_type) {
            case "cursor_runmode_nothing":
                c = "default";
                break;
            case "cursor_runmode_clickme":
                // The "pointer" icon seems the natural choice for "clickme"
                // here, but unfortunately it creates ambiguity with the
                // default editmode pointer icon. Not sure what the best
                // solution is, but for now so we use "default" for clickme.
                // That creates another ambiguity, but it's less of an issue
                // since most of the clickable runtime items are fairly obvious
                // anyway.

                //c = "pointer";

                c = "default";
                break;
            case "cursor_runmode_thicken":
                c = "crosshair";
                break;
            case "cursor_runmode_addpoint":
                c = "cell";
                break;
            case "cursor_editmode_nothing":
                c = "pointer";
                break;
            case "cursor_editmode_connect":
                c = "-webkit-grabbing";
                break;
            case "cursor_editmode_disconnect":
                c = "no-drop";
                break;
            case "cursor_editmode_resize":
                c = "ew-resize";
                break;
            case "cursor_editmode_resize_bottom_right":
                c = "se-resize";
                break;
            case "cursor_scroll":
                c = "all-scroll";
                break;
            case "cursor_editmode_resize_vert":
                c = "ns-resize";
                break;
            case "cursor_editmode_move":
                c = "move";
                break;
            case "cursor_editmode_floating":
                c = "pointer";
                var textentry = get_item(cid, "new_object_textentry");
                if (textentry) {
                    textentry.style.setProperty("cursor", c);
                }
                break;
        }
        patch.style.cursor = c;
        // ico@bukvic.net 2025-10-29:
        // On Win11 this for some reason does not update the cursor
        // until the cursor is moved...
    });
}

// Note: cid can either be a real canvas id, or the string "pd" for the
// console window
function canvas_sendkey(cid, state, evt, char_code, repeat) {
    var shift = 0, repeat_number = 0;
    // ico@vt.edu 2021-08-20: here we check whether evt is not null because we use
    // this same mechanism to paste text into gatom (see m.edit.paste
    // inside the pd_canvas.js)
    if (evt) {
        shift = evt.shiftKey ? 1 : 0,
        repeat_number = repeat ? 1 : 0;
    }
    //post("canvas_sendkey state=" + state + " evt=" + evt +
    //	" char_code=<" + char_code + "> repeat=" + repeat + " shift=" + shift);
    pdsend(cid, "key", state, char_code, shift, 1, repeat_number);
}

exports.canvas_sendkey = canvas_sendkey;

function title_callback(cid, title) {
    patchwin[cid].window.document.title = title;
}

function format_window_title(name, dirty_flag, args, dir) {
        return name + " " + (dirty_flag ? "*" : "") + args + " - " + dir;
}

exports.format_window_title = format_window_title;

// This should be used when a file is saved with the name changed
// (and maybe in other situations)
function gui_canvas_set_title(cid, name, args, dir, dirty_flag) {
    var title = format_window_title(name, dirty_flag, args, dir);
    if (patchwin[cid]) {
        patchwin[cid].title = title;
    } else {
        title_queue[cid] = title;
    }
    populate_open_windows();
}

function query_title_queue(cid) {
    return title_queue[cid];
}

exports.query_title_queue = query_title_queue;

function free_title_queue(cid) {
    title_queue[cid] = null;
}

exports.free_title_queue = free_title_queue;

function window_is_loading(cid) {
    return loading[cid];
}

exports.window_is_loading = window_is_loading;

function set_window_finished_loading(cid, new_win) {
    // ico 2023-08-20: check for dialogs that may be sticking
    // outside the visible desktop area and reposition them
    // accordingly
    //post("set_window_finished_loading " + new_win);
    if (new_win === get_dialogwin(cid) && 
            dialogwin[cid].window.document.
                getElementById("titlebar_title") != null) {
        var object = dialogwin[cid].window.document.
            getElementById("titlebar_title").innerHTML.split(" ")[0]
            .replace('[','').replace(']','');
        dialogwin_check_overflow(new_win, object);
    }
    populate_open_windows();
    loading[cid] = null;
}

exports.set_window_finished_loading = set_window_finished_loading;

// wrapper for nw_create_window
function create_window(cid, type, width, height, xpos, ypos, attr_array) {
    nw_create_window(cid, type, width, height, xpos, ypos, attr_array);
    // initialize variable to reflect that this window has been opened
    loading[cid] = true;
    // we call set_patchwin from the callback in pd_canvas
}

// create a new canvas
// cargs need to be last since they are variable in size
// (these are the patch args, if any)
function gui_canvas_new(cid, width, height, geometry, grid, grid_size_value,
    zoom, editmode, name, dir, dirty_flag, warid, hide_scroll, hide_menu,
    has_toplevel_scalars, isblank, cargs) {

    //post("gui_canvas_new geometry=" + geometry + " w=" + width + " h=" + height);
    //post("gui_canvas_new cid=" + cid + " has_top_level_scalars=" + has_toplevel_scalars);
    // hack for buggy tcl popups... should go away for node-webkit
    //reset_ctrl_on_popup_window
    
    // ico@vt.edu: added has_toplevel_scalars, which is primarily
    // being used for detecting toplevel garrays but may need to be
    // expanded to also deal with scalars
    // 2021-11-12: has_toplevel_scalars now supports:
    //     1 = garray
    //     2 = non-array scalars that need to be scaled on resize
    //post("gui_canvas_new has_toplevel_scalars=" + has_toplevel_scalars);

    // local vars for window-specific behavior
    // visibility of menu and scrollbars, plus canvas background
    scroll[cid] = 1;
    menu[cid] = 1;
    // attempt at getting global presets to play
    // well with local settings.
    var my_canvas_color = "";
    //canvas_color[cid] = orange;
    my_canvas_color = pd_colors["canvas_color"];
    topmost[cid] = 0;
    resize[cid] = 1;
    xscrollable[cid] = 0;
    yscrollable[cid] = 0;
    update_tick[cid] = 0;
    drag_tick[cid] = 0;
    undo[cid] = false;
    redo[cid] = false;
    font[cid] = 10;
    doscroll[cid] = 0;
    showgrid[cid] = grid != 0;
    gridsize[cid] = grid_size_value;
    toplevel_scalars[cid] = has_toplevel_scalars;
    // ico@vt.edu 2022-11-15: by default enable editable until informed
    // otherwise (this message is procesed later via parsing the saved
    // patch or via a message, see g_canvas.c for more info)
    editable[cid] = 1;
    // geometry is just the x/y screen offset "+xoff+yoff"
    geometry = geometry.slice(1);   // remove the leading "+"
    geometry = geometry.split("+"); // x/y screen offset (in pixels)
    // Keep patches on the visible screen
    var xpos = Math.min(Number(geometry[0]), window.screen.width - width);
    var ypos = Math.min(Number(geometry[1]), window.screen.height - height);
    // ico@vt.edu 2022-11-04: for MacOS on nw.js 0.28.3 for some reason
    // the window is not centered vertically against its parent window
    // (Pd-L2Ork console), so here we center it manually. LATER: check
    // if this is necessary for future versions of nw.js and adjust
    // accordingly. Once we implement saving the Pd-L2Ork console location
    // we will want to do this for both xpos and ypos on all OSs.
    // TODO!: do we still need this?
    if (nw_os_is_osx && xpos === 0 && ypos === 22) {
        ypos = window.screen.height/2 - height/2;
    }
    xpos = Math.max(xpos, 0);
    ypos = Math.max(ypos, 0);
    var menu_flag;
    if (menu[cid] == 1) {
        menu_flag = true;
    } else {
        menu_flag = false;
    }
    last_loaded = cid;

    // Not sure why resize and topmost are here--but we'll pass them on for
    // the time being...
    // ico@vt.edu 2020-08-24: this is because in 1.x we can change these window
    // properties via scripting. We should add this to 2.x soon...
    //post("gui_canvas_new cargs=" + cargs);
    create_window(cid, "pd_canvas", width + (170 * k12_mode * isblank),
        height + (100 * k12_mode * isblank), xpos, ypos, {
            menu_flag: menu_flag,
            resize: resize[cid],
            topmost: topmost[cid],
            color: my_canvas_color,
            name: name,
            dir: dir,
            dirty: dirty_flag,
            warid: warid,
            args: cargs,
            zoom: zoom,
            editmode: editmode,
            hide_scroll: hide_scroll,
            isblank: isblank
    });
}

/* This gets sent to Pd to trigger each object on the canvas
   to do its "vis" function. The result will be a flood of messages
   back from Pd to the GUI to draw these objects */
function canvas_map(name) {
    console.log("canvas mapping " + name + "...");
    pdsend(name + " map 1");
}

function gui_canvas_erase_all_gobjs(cid) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        var elem;
        while (elem = svg_elem.firstChild) {
            svg_elem.removeChild(elem);
        }
    });
}

exports.canvas_map = canvas_map;

// Start Pd

// If the GUI is started first (as in a Mac OSX Bundle) we use this
// function to actually start the core
function spawn_pd(gui_path, port, file_to_open) {
    post("gui_path is " + gui_path);
    var pd_binary,
        platform = process.platform,
        flags = ["-guiport", port];
    if (platform === "darwin") {
        // OSX -- this is currently tailored to work with an app bundle. It
        // hasn't been tested with a system install of pd-l2ork
        pd_binary = path.join(gui_path, "bin", "pd-l2ork");
        if (file_to_open) {
            flags.push("-open", file_to_open);
        }
    } else {
        pd_binary = path.join(gui_path, "..", "bin", "pd-l2ork");
        flags.push("-nrt"); // for some reason realtime causes watchdog to die
    }
    post("binary is " + pd_binary);
    // AG: It isn't nice that we change the cwd halfway through the startup
    // here, but since the GUI launches the engine if we come here (that's how
    // it works on the Mac), we *really* want to launch the engine in the
    // user's home directory and not in some random subdir of the OSX app
    // bundle. Note that to make that work, the pd-l2ork executable needs to
    // be invoked using an absolute path (see above).
    process.chdir(process.env.HOME);
    var child = cp.spawn(pd_binary, flags, {
        stdio: "inherit",
        detached: true
    });
    child.on("error", function(err) {
        pd_window.alert("Couldn't successfully start Pd due to an error:\n\n" +
          err + "\n\nClick Ok to close Pd.");
        process.exit(1);
    });
    child.unref();
    post("Pd started.");
}

// net stuff
var net = require("net");

var HOST = "127.0.0.1";
var PORT;
var connection; // the GUI's socket connection to Pd

exports.set_port = function (port_no) {
    PORT = port_no;
}

var secondary_pd_engines = {};

// This is an alarmingly complicated and brittle approach to opening
// files from a secondary instance of Pd in a currently running instance.
// It works something like this:
// 1. User is running an instance of Pd-L2Ork.
// 2. User runs another instance of Pd-L2Ork from the command line, specifying
//    files to be opened as command line args. Or, they click on a file which
//    in the desktop or file manager which triggers the same behavior.
// 2. A new Pd process starts-- let's call it a "secondary pd engine".
// 3. The secondary pd engine tries to run a new GUI.
// 4. The secondary GUI forwards an "open" message to the currently running GUI.
// 5. The secondary GUI exits (before spawning any windows).
// 6. The original GUI receives the "open" message, finds the port number
//    for the secondary Pd engine, and opens a socket to it.
// 7. The original GUI receives a message to set the working directory to
//    whatever the secondary Pd engine thinks it should be.
// 8. The original GUI sends a message to the secondary Pd instance, telling
//    it to send a list of files to be opened.
// 9. The original GUI receives a message from the secondary Pd instance
//    with the list of files.
// 10.For each file to be opened, the original GUI sends a message to the
//    _original_ Pd engine to open the file.
// 11.Once these messages have been sent, the original GUI sends a message
//    to the secondary Pd engine to quit.
// 12.The original Pd engine opens the files, and the secondary Pd instance
//    quits.
function connect_as_client_to_secondary_instance(host, port, pd_engine_id) {
    //post("connect_as_client_to_secondary_instance " + pd_engine_id);
    var client = new net.Socket(),
        command_buffer = {
            next_command: ""
    };
    client.setNoDelay(true);
    // prepended + forces number format
    client.connect(+port, host, function() {
        console.log("CONNECTED TO: " + host + ":" + port);
        secondary_pd_engines[pd_engine_id] = {
            socket: client
        }
        client.write("pd forward_files_from_secondary_instance;");
    });
    client.on("data", function(data) {
        //post("...on data:\n" + data + "\n\n" + command_buffer);
        // Terrible thing:
        // We're parsing the data as it comes in-- possibly
        // from multiple ancillary instances of the Pd engine.
        // So to retain some semblance of sanity, we only let the
        // parser evaluate commands that we list in the array below--
        // anything else will be discarded.  This is of course bad
        // because it means simple changes to the code, e.g., changing
        // the name of the function "gui_set_cwd" would cause a bug
        // if you forget to come here and also change that name in the
        // array below.
        // Another terrible thing-- gui_set_cwd sets a single, global
        // var for the working directory. So if the user does something
        // weird like write a script to open files from random directories,
        // there would be a race and it might not work.
        // Yet another terrible thing-- now we're setting the current
        // working directory both in the GUI, and from the secondary instances
        // with "gui_set_cwd" below.
        perfect_parser(data, command_buffer, [
            "gui_set_cwd",
            "gui_open_via_unique"
        ]);
    });
    client.on("close", function () {
        // I guess somebody could script opening patches in an
        // installation, so let's go ahead and delete the key here
        // (The alternative is just setting it to undefined)
        delete secondary_pd_engines[pd_engine_id];
    });
}

function quit_secondary_pd_instance (pd_engine_id) {
    secondary_pd_engines[pd_engine_id].socket.write("pd quit;");
}

// This is called when the running GUI receives an "open" event.
exports.connect_as_client_to_secondary_instance =
    connect_as_client_to_secondary_instance;

function connect_as_client() {
    var client = new net.Socket();
    client.setNoDelay(true);
    // uncomment the next line to use fast_parser (then set its callback below)
    //client.setEncoding("utf8");
    client.connect(PORT, HOST, function() {
        console.log("CONNECTED TO: " + HOST + ":" + PORT);
    });
    connection = client;
    init_socket_events();
}

exports.connect_as_client = connect_as_client;

function connect_as_server(gui_path, file_path) {
    var server = net.createServer(function(c) {
            post("incoming connection to GUI");
            connection = c;
            init_socket_events();
        }),
        port = PORT,
        ntries = 0,
        listener_callback = function() {
            post("GUI listening on port " + port + " on host " + HOST);
            spawn_pd(gui_path, port, file_path);
        };
    server.listen(port, HOST, listener_callback);
    // try to reconnect if necessary
    server.on("error", function (e) {
        if (e.code === "EADDRINUSE" && ntries++ < 20) {
            post("Address in use, retrying...");
            port++;
            setTimeout(function () {
                server.close();
                server.listen(port, HOST); // (already have the callback above)
            }, 30); // Not sure we really need a delay here
        } else {
            pd_window.alert("Error: couldn't bind to a port. Either port nos " +
                  PORT + " through " + port + " are taken, or you don't have " +
                  "networking turned on. (See Pd's html doc for details.)");
            server.close();
            process.exit(1);
        }
    });
}

exports.connect_as_server = connect_as_server;

// Add a 'data' event handler for the client socket
// data parameter is what the server sent to this socket

// We're not receiving FUDI (i.e., Pd) messages. Right now we're just using
// the unit separator (ASCII 31) to signal the end of a message. This is
// easier than checking for unescaped semicolons, since it only requires a
// check for a single byte. Of course this makes it more brittle, so it can
// be changed later if needed.

function perfect_parser(data, cbuf, sel_array) {
        var i, len, selector, args;
        len = data.length;
        for (i = 0; i < len; i++) {
            // check for end of command:
            if (data[i] === 31) { // unit separator
                // decode next_command
                try {
                    // This should work for all utf-8 content
                    cbuf.next_command =
                        decodeURIComponent(cbuf.next_command);
                }
                catch(err) {
                    // This should work for ISO-8859-1
                    cbuf.next_command = unescape(cbuf.next_command);
                }
                // Turn newlines into backslash + "n" so
                // eval will do the right thing with them
                cbuf.next_command = cbuf.next_command.replace(/\n/g, "\\n");
                cbuf.next_command = cbuf.next_command.replace(/\r/g, "\\r");
                selector = cbuf.next_command.slice(0, cbuf.next_command.indexOf(" "));
                args = cbuf.next_command.slice(selector.length + 1);
                cbuf.next_command = "";
                // Now evaluate it
                //post("Evaling: " + selector + "(" + args + ");");
                // For communicating with a secondary instance, we filter
                // incoming messages. A better approach would be to make
                // sure that the Pd engine only sends the gui_set_cwd message
                // before "gui_startup".  Then we could just check the
                // Pd engine id in "gui_startup" and branch there, instead of
                // fudging with the parser here.
                if (!sel_array || sel_array.indexOf(selector) !== -1) {
                    eval(selector + "(" + args + ");");
                }
            } else {
                cbuf.next_command += "%" +
                    ("0" // leading zero (for rare case of single digit)
                     + data[i].toString(16)) // to hex
                       .slice(-2); // remove extra leading zero
            }
        }
    };

function init_socket_events () {
    // A not-quite-FUDI command: selector arg1,arg2,etc. These are
    // formatted on the C side to be easy to parse here in javascript
    var command_buffer = {
        next_command: ""
    };
    connection.on("data", function(data) {
        perfect_parser(data, command_buffer);
    });
    connection.on("error", function(e) {
        console.log("Socket error: " + e.code);
        nw_app_quit();
    });

    // Add a "close" event handler for the socket
    connection.on("close", function() {
        //console.log("Connection closed");
        //connection.destroy();
        nw_app_quit(); // set a timeout here if you need to debug
    });
}

exports.init_socket_events = init_socket_events;

// Send commands to Pd
function pdsend() {
    // Using arguments in this way disables V8 optimization for
    // some reason.  But it doesn't look like it makes that much
    // of a difference
    var string = Array.prototype.join.call(arguments, " ");
    connection.write(string + ";");
    // reprint the outgoing string to the pdwindow
    //post(string + ";", "red");
}

exports.pdsend = pdsend;

// Send a ping message back to Pd
function gui_ping() {
    pdsend("pd ping");
}

// Send a message to Pd to ping the "watchdog", which is a program
// that supervises Pd when run with -rt flag on some OSes
function gui_ping_watchdog() {
    pdsend("pd watchdog");
}

// Schedule watchdog pings for the life of the GUI
function gui_watchdog() {
    setInterval(gui_ping_watchdog, 2000);
}

// Text drawing stuff

// Here's the main API, structured to make an easier (inital) transition
// from tcl/tk to javascript

// Gobj container, so that all drawn items are contained in a <g> which
// handles displacing (and in the future, possibly clicks and other events)
function get_gobj(cid, object) {
    return patchwin[cid].window.document.getElementById(object + "gobj");
}

// Convenience function to get a drawn item of gobj
function get_item(cid, item_id) {
    return patchwin[cid].window.document.getElementById(item_id);
}

// Convenience function to determine whether the window has posted yet
function check_cid(cid) {
    if (patchwin[cid])
        return 1;
    else
        return 0;
}

// Similar to [canvas create] in tk
function create_item(cid, type, args) {
    var item = patchwin[cid].window.document
        .createElementNS("http://www.w3.org/2000/svg", type);
    if (args !== null) {
        configure_item(item, args);
    }
    return item;
}

// Similar to [canvas itemconfigure], without the need for a reference
// to the canvas
function configure_item(item, attributes) {
    // draw_vis from g_template sends attributes
    // as a ["attr1",val1, "attr2", val2, etc.] array,
    // so we check for that here
    var value, i, attr;
    if (Array.isArray(attributes)) {
        // we should check to make sure length is even here...
        for (i = 0; i < attributes.length; i+=2) {
            value = attributes[i+1];
            item.setAttributeNS(null, attributes[i],
                Array.isArray(value) ? value.join(" "): value);
        }
    } else {
        for (attr in attributes) {
            if (attributes.hasOwnProperty(attr)) {
                item.setAttributeNS(null, attr, attributes[attr]);
            }
        }
    }
}

// The GUI side probably shouldn't know about "items" on SVG.
function gui_configure_item(cid, tag, attributes) {
    gui(cid).get_elem(tag, attributes);
}

function add_gobj_to_svg(svg, gobj) {
    svg.insertBefore(gobj, svg.querySelector(".cord"));
}

// New interface to incrementally move away from the tcl-like functions
// we're currently using. Basically like a trimmed down jquery, except
// we're dealing with multiple toplevel windows so we need an window id
// to get the correct Pd canvas context. Also, some of Pd's t_text tags
// still have "." in them which unfortunately means we must wrap
// getElementById instead of the more expressive querySelector[All] where
// the "." is interpreted as a CSS class selector.

// Methods:
// get_gobj(id, callbackOrObject) returns a reference to this little canvas
//                                interface
// get_elem(id, callbackOrObject) returns a reference to this little canvas
//                                interface
// get_nw_window(callback)        returns a reference to the nw.js Window
//                                wrapper. We keep this separate from the
//                                others in order to annotate those parts
//                                of the code which rely on the nw API (and
//                                abstract them out later if we need to)

// objects are used to set SVG attributes (in the SVG namespace)
// function callbacks have the following args: (DOMElement, window)

// Note about checking for existence:
// Why? Because an iemgui inside a gop canvas will send drawing updates,
// __even__ __if__ that iemgui is outside the bounds of the gop and thus
// not displayed. This would be best fixed in the C code, but I'm not
// exactly sure where or how yet.
// Same problem on Pd Vanilla, except that tk canvas commands on
// non-existent tags don't throw an error.

var gui = (function() {
    var c = {}; // object to hold references to all our canvas closures
    // We store the last "thing" we fetched from the window. This is either
    // the window itself or a "gobj". Regular old DOM elements that aren't
    // a "gobj" container don't count. This way we can do a "get_gobj" then
    // gang multiple element queries after it that work within our last
    // "gobj." (Same for window.)
    var last_thing;
    var null_fn, null_canvas;
    var create_canvas = function(cid, w) {
        var get = function(parent, sel, arg, suffix) {
            sel = sel + (suffix ? "gobj" : "");
            var elem = parent ?
                parent.querySelector(sel) :
                w.window.document.getElementById(sel);
            last_thing = parent ? last_thing : elem;
            if (elem) {
                if (arg && typeof arg === "object") {
                    configure_item(elem, arg);
                } else if (typeof arg === "function") {
                    arg(elem, w.window, c[cid]);
                }
            }
            return c[cid];
        }
        return {
            append: !w ? null_fn: function(cb) {
                var frag = w.window.document.createDocumentFragment();
                frag = cb(frag, w.window, c[cid]);
                last_thing.appendChild(frag);
                return c[cid];
            },
            get_gobj: !w ? null_fn : function(sel, arg) {
                return get(null, sel, arg, "gobj");
            },
            get_elem: !w ? null_fn : function(sel, arg) {
                return get(null, sel, arg);
            },
            get_nw_window: !w ? null_fn : function(cb) {
                cb(w);
                return c[cid];
            },
            q: !w ? null_fn : function(sel, arg) {
                return last_thing ? get(last_thing, sel, arg) : c[cid];
            },
            debug: function() { return last_thing; }
        }
    };
    // The tcl/tk interface ignores calls to configure non-existent items on
    // canvases. Additionally, its interface was synchronous so that the window
    // would always be guaranteed to exist. So we create a null canvas to keep
    // from erroring out when things don't exist.
    null_fn = function() {
        return null_canvas;
    }
    null_canvas = create_canvas(null);
    var canvas_container = function(cid) {
        last_thing = c[cid] ? c[cid] : null_canvas;
        return last_thing;
    }
    canvas_container.add = function(cid, nw_win) {
        c[cid] = create_canvas(cid, nw_win);
    }
    canvas_container.remove = function(cid, nw_win) {
        c[cid] = null;
    }
    return canvas_container;
}());

// For debugging
exports.gui = gui;

var index_file_lines;      // for a better search for tooltips
                           // text file into lines of text
var index_file_lines_path; // keeps only the last entry that deals
                           // with full path of each object

var _tooltip_obj  = 3;
var _tooltip_nlet = 6;

// helper function to get the tooltip data for the object obj
// which denotes either object itself (3), or nlets (6), see
// static variables above _tooltip_obj and _tooltip_nlet that
// are defined for this purpose
function find_tooltip_index_line(tip, obj) {
    // if the index has not been built yet
    if (!index_file_lines_path) {
        //post("index_file_lines_path empty");
        return;
    }

    var line = -1;
    var i;

    // first clean up the tip, getting rid of potential creation arguments
    var tipname = tip.replace(/ .*/,'');
    // now replace '/' with ' '
    // we check for the RPi nwjs version since as of 2023-08-14 that one
    // lacks the replaceAll function, and provide our own
    // RPI-LATER: update this once we update the RPi nwjs version to a newer one
    if (check_nw_version("0.28"))
        tipname = String(tipname).split("/").join(" ");
    else
        tipname = String(tipname).replaceAll("/"," ");
    // add space before it, to prevent false positives (e.g. image is found in drawimage)
    tipname = " " + tipname + "-help";
    for (i = 0; i < index_file_lines_path.length; i++) {
        if (index_file_lines_path[i].includes(tipname)) {
            line = i;
            //post("...found it at " + line);
            break;
        }
    }
    //post("#1 line=" + line + " tipname=" + tipname);
    if (line === -1) {
        // if we haven't found anything using tip var, try the obj var
        // first clean up the obj, getting rid of potential creation arguments
        var objname = obj.replace(/ .*/,'');
        // now replace '/' with ' '
        // we check for the RPi nwjs version since as of 2023-08-14 that one
        // lacks the replaceAll function, and provide our own
        // RPI-LATER: update this once we update the RPi nwjs version to a newer one
        if (check_nw_version("0.28"))
            objname = String(objname).split("/").join(" ");
        else
            objname = String(objname).replaceAll("/"," ");
        // add space before it, to prevent false positives (e.g. image is found in drawimage)
        //objname = " " + objname + "-help";
        for (i = 0; i < index_file_lines_path.length; i++) {
            if (index_file_lines_path[i].includes(objname)) {
                line = i;
                //post("...found it at " + line);
                break;
            }
        }
    }
    //post("#2 line=" + line + " objname=<" + objname + ">");
    return line;    
}


// Most of the following functions map either to pd.tk procs, or in some cases
// tk canvas subcommands

// The "gobj" is a container for all the shapes/paths used to display
// a graphical object on the canvas. This comes in handy-- for example, we
// can displace an object just by translating its "gobj".

// Object, message, and xlet boxes should be crisp (i.e., no anti-aliasing),
// and the "shape-rendering" attribute of "crispEdges" acheives this. However,
// that will also create asymmetric line-widths when scaling-- for example,
// the left edge of a rect may be 3 pixels while the right edge is 4. I'm not
// sure whether this is a bug or just the quirky behavior of value "crispEdges".
// As a workaround, we explicitly add "0.5" to the gobj's translation
// coordinates below. This aligns the shapes-- lines, polygons, and rects
// with a 1px stroke-- to the pixel grid, making them crisp.

// Also-- note that we have a separate function for creating a scalar.
// This is because the user may be drawing lines or paths as
// part of a scalar, in which case we want to leave it up to them to align
// their drawing to the pixel grid. For example, imagine a user pasting a
// path command from the web. If that path already employs the "0.5" offset
// to align to the pixel-grid, a gobj offset would cancel it out. That
// would mean the Pd user always has to do the _opposite_ of what they read
// in SVG tutorials in order to get crisp lines, which is bad.
// In the future, it might make sense to combine the scalar and object
// creation, in which case a flag to toggle the offset would be appropriate.

/* ico@vt.edu 20200916: added following params:
   ownercid    = the canvas id to which gobj belongs. This is the same as cid
                 for toplevel gobjs and for non-toplevel gop-drawn objects, it
                 is the actual canvas the gobj belongs to. cid on the other hand
                 references the canvas on which gobj is being drawn and for gop-
                 drawn objects that may be a parent or even several levels above
                 the parent (e.g. toplevel->gop1->gop2 that is visible inside
                 gop1, etc.).

   parentcid   = the immediate parent of the ownercid. This is needed to identify
                 appropriate class context below when we wish to reference the right
                 nested_gop and tgt below.

   is_toplevel = behaves the same as the is_toplevel inside c code with one notable
                 exception: when drawing a gop graph, is_toplevel means the gop being
                 drawn is exactly one level below the toplevel, so that it can be
                 drawn as a GOP graph. This is critical for the new GOP implementation
                 that truly draws a window inside a GOP subpatch/abstraction instead
                 of the old klunky implementation that only draws items that fit
                 inside GOP, except that it also draws scalars and other things
                 happily outside of its bounds. IMPORTANT: this variable is now present
                 in many (most?) drawing commands as it allows us to differentiate
                 between the two contexts and offers a convenient way to filter objects
                 whose selection border should be highlighted when they are selected.
*/
function gui_gobj_new(cid, ownercid, parentcid, tag, type, xpos, ypos, is_toplevel, is_canvas_obj, objname, tipname) {
    var g, draw_xpos, draw_ypos;
    draw_xpos = xpos;
    draw_ypos = ypos;

    //post("gui_gobj_new cid=" + cid + " tag=" + tag + " objname=" + objname + " tipname=" + tipname);
    if(tipname==null){
        tipname="fixme";
    }
    tipname = tipname.trim();
    if (type === "graph") { // graph object
        /*
        post("===\ngui_gobj_new GRAPH drawon=" + cid + " ownercid=" + ownercid +
            " parentcid=" + parentcid + " tag=" + tag + " type=" + type +
            " xpos=" + xpos + " ypos=" + ypos + " is_toplevel=" + is_toplevel +
            " is_canvas_obj=" + is_canvas_obj);
        */

        /*
        post("...GOP svg_elem=" + (is_toplevel === 1 ? "patchsvg" : 
            (parentcid === cid ? "[ownercid]" + ownercid + "svg" :
                "[parentcid]" + parentcid + "svg")));
        */
        gui(cid).get_elem("patchsvg", function(svg_elem, w) {
            var nested_gop;

            if (is_toplevel === 0) {
                nested_gop = w.document.getElementsByClassName(
                    (parentcid === cid ? ownercid + "svg" : parentcid + "svg"));
                var svg_view_box = svg_elem.getAttribute("viewBox").split(" ");
                draw_xpos = draw_xpos - nested_gop[0].getScreenCTM().e +
                    svg_elem.getBoundingClientRect().left - svg_view_box[0];
                draw_ypos = draw_ypos - nested_gop[0].getScreenCTM().f +
                    svg_elem.getBoundingClientRect().top - svg_view_box[1];
                //post("......offset x=" + draw_xpos + " y=" + draw_ypos);
            }

            draw_xpos += 0.5;
            draw_ypos += 0.5;

            var transform_string = "matrix(1,0,0,1," + draw_xpos + "," + draw_ypos + ")";
            // we make graph into another svg elem to allow for clipping
            g = create_item(cid, "g", {
                id: tag + "gobj",
                transform: transform_string,
                class: type + 
                    (is_toplevel === 0 ? "" : " gop") + 
                    (type == "graph" ? " " + ownercid : "") +
                    (is_canvas_obj === 0 ? "" : " canvasobj"),
                obj_text: objname,
                tip_text: tipname
            });
            var s = create_item(cid, "svg", {
                id: tag + "svg",
                class: (type == "graph" ? "graphsvg " + ownercid + "svg" : "")
            });
            add_gobj_to_svg((is_toplevel === 1 ? svg_elem : nested_gop[0]), g);
            g.appendChild(s);

            //post("gui_gobj_new graph tipname=<" + tipname + "> objname=<" + objname + ">");
            var x = document.createElementNS("http://www.w3.org/2000/svg", "title");

            // here we need to use tipname as objname may resolve into something that
            // does not reflect the actual object (e.g. help file for pd could be canvas)
            var line = find_tooltip_index_line(tipname, objname);
            if (line > -1) {
                var yyy = index_file_lines[line].split(';:')[_tooltip_obj];
                var parsed_tooltip = String(yyy).replace(/\\/g,'');
                x.textContent = parsed_tooltip;

                g.appendChild(x);
                g.setAttribute("tooltip", parsed_tooltip);
            }
       });
   } else { // non-graph object (can be inside a GOP, tested with is_toplevel)
        //post("...OBJECT classname=" + ownercid + "svg");
        /*
        post("===\ngui_gobj_new OBJECT drawon=" + cid + " ownercid=" + ownercid +
            " parentcid=" + parentcid + " tag=" + tag + " type=" + type +
            " xpos=" + xpos + " ypos=" + ypos + " is_toplevel=" + is_toplevel +
            " is_canvas_obj=" + is_canvas_obj);
        */
        gui(cid).get_elem("patchsvg", function(svg_elem, w) {
            var tipFirstArg = tipname.replace(/ .*/,'');
            // post("ID=<" + tipname + "> <" + tipFirstArg + ">");
            var transform_string;
            if (is_toplevel === 0) {
                var tgt = w.document.getElementsByClassName(ownercid + "svg");
                var svg_view_box = svg_elem.getAttribute("viewBox").split(" ");

                draw_xpos = draw_xpos - tgt[0].getScreenCTM().e +
                    svg_elem.getBoundingClientRect().left - svg_view_box[0];
                draw_ypos = draw_ypos - tgt[0].getScreenCTM().f +
                    svg_elem.getBoundingClientRect().top - svg_view_box[1];
                //post("......offset x=" + draw_xpos + " y=" + draw_ypos);
            }

            draw_xpos += 0.5;
            draw_ypos += 0.5;

            transform_string = "matrix(1,0,0,1," + draw_xpos + ", " + draw_ypos + ")";  
            g = create_item(cid, "g", {
                id: tag + "gobj",
                transform: transform_string,
                class: type +
                    (is_canvas_obj === 0 ? "" : " canvasobj"),
                //orig_xpos: xpos,
                //orig_ypos: ypos,
                obj_text: objname,
                tip_text: tipname
            });

            // ico 2023-07-18: here we can only allow tooltips for toplevel
            // objects. this is currently disabled, as it limits customization
            // of tooltips at runtime for each object inside a GOP subpatch
            // or abstraction.
            // ico 2023-07-21: we only allow tooltips for toplevel objects
            // because otherwise all the background objects may potentially
            // prevent the most important graph-on-parent tooltip
            //post("objname=" + objname + " tipname=" + tipname);
            if (objname != null && is_toplevel === 1) {
                var x = document.createElementNS("http://www.w3.org/2000/svg", "title");
                // here we need to use tipname as objname may resolve into something that
                // does not reflect the actual object (e.g. help file for inlet is pd)
                var line = find_tooltip_index_line(tipname, objname);
                if (line > -1) {
                    //post("objname: " + objname +
                    //    " object description: " + index_file_lines[line]);
                    var yyy = index_file_lines[line].split(';:')[_tooltip_obj];
                    var parsed_tooltip = String(yyy).replace(/\\/g,'');
                    x.textContent = parsed_tooltip;
                    g.appendChild(x);
                    g.setAttribute("tooltip", parsed_tooltip);
                }
            }
            if (is_toplevel === 0) {
                //post("GOP appendChild")
                tgt[0].appendChild(g);
            } else {
                //post("add_gop_to_svg");
                add_gobj_to_svg(svg_elem, g);
            }
        });
    }
    return g;
}

// ico@vt.edu 2022-11-09: implementation for the gopspill feature used by the K12 mode
function gui_graph_gopspill(cid, tag, state) {
    //post("gui_graph_gopspill cid=" + cid + " tag=" + tag + " state=" + state);
    gui(cid).get_elem(tag + "svg", function(graph_svg) {
        graph_svg.style.setProperty("overflow", state ? "visible" : "hidden");
    });
    //gui(cid).get_nw_window(function(nw_win) {
    //    var svg_elem = nw_win.window.document.getElementById("patchsvg");
    gui(cid).get_elem(tag + "gobj", function(graph_gobj) {
        // here we have to reference specifically object that is direct
        // child of the graph object. this is because in situations where
        // we draw gop inside gop and look just for .gopborder, we will
        // get the first one which is not our own, but that of our child gop.
        // see below for a visual representation of this.
        var border = graph_gobj.querySelectorAll(":scope > .gopborder");
        //post("border=" + border[0]);
        /*
        post("graph_gobj=" + graph_gobj + " border=" + border +
             " prev=" + border.previousElementSibling +
             " last=" + graph_gobj.lastChild);
        */
        // now, if we have gopspill enabled, push border immediately below the main <g>
        // and above the <svg> element, so that the border is behind spilled objects.
        // if we have gopspill disabled, push it back below the <svg> element, so that
        // it is over any edge objects. Note that DOM objects further down the file are
        // drawn visually above those who are above them in the file, e.g:
        // <body>
        // ...
        // <g>
        //   <svg>
        //   <rect> <-- this one is drawn above the <svg>
        // etc.
        if (state)
        {
            graph_gobj.insertBefore(border[0], graph_gobj.firstChild);
        } else {
            // for some reason nextElementSibling or nextSibling returns null
            // so we go by the lastChild which are the nlets
            graph_gobj.insertBefore(border, graph_gobj.lastChild);
        }
    });
}

function gui_text_draw_border(cid, tag, bgcolor, isbroken, width, height, is_toplevel) {
    // post("===\ngui_text_draw_border is_toplevel=" + is_toplevel);
    var isgop = 0;
    gui(cid).get_gobj(tag, function(e) {
        if(e.classList.contains("graph")) {
            isgop = 1;
        }
    });
    if (isgop) {
        gui(cid).get_elem(tag + "svg", function(e) {
            e.setAttribute("width", width);
            e.setAttribute("height", height);
        });
    }
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        // isbroken means either
        //     a) the object couldn't create or
        //     b) the box is empty
        var rect = create_item(cid, "rect", {
            width: width,
            height: height,
            //"shape-rendering": "crispEdges",
            class: (is_toplevel === 1 ? "toplevel " : "") + 
                (isgop === 1 ? "gopborder" : "border"),
            id: tag + "border"
        });
        if (isbroken === 1) {
            rect.classList.add("broken_border");
        }
        frag.appendChild(rect);
        return frag;
    });
}

function gui_gobj_draw_io(cid, parenttag, tag, x1, y1, x2, y2, basex, basey,
    type, i, is_signal, is_iemgui) {
    //post("gui_gobj_draw_io");

    gui(cid).get_gobj(parenttag)
    .append(function(frag) {
        var xlet_class, xlet_id, rect;
        if (is_iemgui) {
            xlet_class = "xlet_iemgui";
            // We have an inconsistency here.  We're setting the tag using
            // string concatenation below, but the "tag" for iemguis arrives
            // to us pre-concatenated.  We need to remove that formatting in c,
            // and in general try to simplify tag creation on the c side as
            // much as possible.
            xlet_id = tag;
        } else if (is_signal) {
            xlet_class = "xlet_signal";
            xlet_id = tag + type + i;
        } else {
            xlet_class = "xlet_control";
            xlet_id = tag + type + i;
        }
        rect = create_item(cid, "rect", {
            width: x2 - x1,
            height: y2 - y1,
            x: x1 - basex,
            y: y1 - basey,
            id: xlet_id,
            class: xlet_class,
            //"shape-rendering": "crispEdges"
        });
        //post("gui_gobj_draw_io parenttag=" + parenttag);
        gui(cid).get_gobj(parenttag, function(e) { //lilykhoch, redesigned by ico
            //post("...e="+ e);
            var x = document.createElementNS("http://www.w3.org/2000/svg", "title");
            var parent_obj_text = e.getAttribute('obj_text');
            if(parent_obj_text == null) {
                parent_obj_text = "fixme";
            }
            var parent_tip_text = e.getAttribute('tip_text');
            if(parent_tip_text == null) {
                parent_tip_text = "fixme";
            }           
            var line = find_tooltip_index_line(parent_tip_text, parent_obj_text);
            if (line > -1) {
                var yyy = index_file_lines[line].split(';:')[_tooltip_nlet];
                if (yyy != null) {
                    var inlet = yyy.match(/(?<=I.\s)[^\|]+/g);
                    var outlet = yyy.match(/(?<=O.\s)[^\|]+/g);
                    var inlet_outlet = yyy.match(/(?<=\|)[^|]*(?=\|)/g);
                    var rectId= rect.getAttribute('id').slice(-2); //id of rectangle nlet
                    if (inlet_outlet) {
                        for (var i = 0; i < inlet_outlet.length; i++) { //Case 1: I0,I2,O1,O2,...
                            var nlet_id = inlet_outlet[i].match(/^[^ ]*/); //id of the nlet in tooltip array
                            if(nlet_id == rectId.toUpperCase()) {
                                x.textContent = inlet_outlet[i].match(/(?<= )(.*)/)[0].replace(/\\/g,'');
                                break;
                            }
                        }
                        if(x.textContent == ""){//Case 2:IN,ON
                            for (var i = 0; i < inlet_outlet.length; i++) {
                                var nlet_id = inlet_outlet[i].match(/^[^ ]*/); //id of the nlet in tooltip array
                                if(nlet_id == "IN" && rectId.charAt(0).toUpperCase() == "I") {
                                    x.textContent= inlet[inlet.length-1].replace(/\\/g,'');
                                    break;
                                }
                                else if( nlet_id == "ON" && rectId.charAt(0).toUpperCase() == "O") {
                                    x.textContent= outlet[outlet.length-1].replace(/\\/g,'');
                                    break;
                                }
                            }
                        }
                    }
                }

                if(x.textContent==""){     //Case 3: no tooltip
                    x.textContent="";
                }
                rect.appendChild(x);
            }
        });
        // rect.appendChild(x);
        frag.appendChild(rect);
        return frag;
    });
}

function gui_gobj_redraw_io(cid, parenttag, tag, x, y, type, i, basex, basey) {
    // We have to check for null. Here's why...
    // if you create a gatom:
    //   canvas_atom -> glist_add -> text_vis -> glist_retext ->
    //     rtext_retext -> rtext_senditup ->
    //       text_drawborder (firsttime=0) -> glist_drawiofor (firsttime=0)
    // This means that a new gatom tries to redraw its inlets before
    // it has created them.
    gui(cid).get_elem(tag + type + i, {
        x: x - basex,
        y: y - basey
    });
}

function gui_gobj_erase_io(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_gobj_configure_io(cid, tag, is_iemgui, is_signal, width) {
    gui(cid).get_elem(tag, {
        "stroke-width": width
    })
    .get_elem(tag, function(e) {
        var type;
        if (is_iemgui) {
            type = "xlet_iemgui";
        } else if (is_signal) {
            type = "xlet_signal";
        } else {
            "xlet_control";
        }
        e.classList.add(type);
        e.classList.remove("xlet_selected");
        e.classList.remove("xlet_disabled");
    });
}

function gui_gobj_highlight_io(cid, tag, enabled) {
    gui(cid).get_elem(tag, function(e) {
        e.classList.add("xlet_selected");
        if (enabled == 0)
        {
            e.classList.add("xlet_disabled");
        }
    });
}

function message_border_points(width, height) {
    return [0,0,
            width+4, 0,
            width, 4,
            width, height-4,
            width+4, height,
            0, height,
            0, 0]
        .join(" ");
}

// called from pd_canvas.js text events to deal with 
// the drawing of the msg box
function gui_message_update_textarea_border(cid, elem, init_width) {
	if (elem.classList.contains("msg")) {
        /*
        var rect = elem.getBoundingClientRect();
        var width = rect["right"] - rect["x"];
        var height = rect["bottom"] - rect["y"];
        */
        //post("msg border <"+width+" "+height+">");
		/*if (init_width) {
			var i, ncols = 0,
			    text = elem.innerText,
			    textByLine = text.split(/\r*\n/);
			for (i = 0; i < textByLine.length; i++) {
				if (textByLine[i].length > ncols) {
					ncols = textByLine[i].length;
                    //post("ncols="+ncols);
				}
			}
            var minw = parseInt(elem.style.minWidth);
            var spec = elem.getAttribute("width_spec");
            if(ncols < 3 && spec <= 0)
                ncols = 3;
            if(spec > 0)
                ncols = spec;
            else if (ncols > 60)
                ncols = 60;
			//configure_item(elem, {
	        //    cols: ncols
        	//});
        	//gui_gobj_erase_io(elem.getAttribute("cid"), elem.getAttribute("tag"));
		}*/
		gui_message_redraw_border(
			elem.getAttribute("cid"),
			elem.getAttribute("tag"),
			elem.offsetWidth + 4,
			parseInt(elem.offsetHeight / elem.getAttribute("font_height")) *
                elem.getAttribute("font_height") + 4
			);
        do_getscroll(cid, 1);
	}
}

exports.gui_message_update_textarea_border = gui_message_update_textarea_border;

function gui_message_draw_border(cid, tag, width, height) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var polygon = create_item(cid, "polygon", {
            points: message_border_points(width, height),
            fill: "none",
            stroke: "black",
            // message is not visible inside GOP, so we don't have to worry about
            // whether it is toplevel object. we should never get here if it is not.
            class: "toplevel border"
            //id: tag + "border"
        });
        frag.appendChild(polygon);
        return frag;
    });
}

function gui_message_flash(cid, tag, state) {
    gui(cid).get_gobj(tag, function(e) {
        if (state !== 0) {
            e.classList.add("flashed");
        } else {
            e.classList.remove("flashed");
        }
    });
}

function gui_message_redraw_border(cid, tag, width, height) {
    gui(cid).get_gobj(tag)
    .q(".border", {
        points: message_border_points(width, height)
    });
}

function atom_border_points(width, height, is_dropdown) {
    // For atom, angle the top-right corner.
    // For dropdown, angle both top-right and bottom-right corners
    var bottom_right_x = is_dropdown ? width - 4 : width;
    return  [0, 0,
            width - 4, 0,
            width, 4,
            width, height - 4,
            bottom_right_x, height,
            0, height,
            0, 0]
        .join(" ");
}

function atom_arrow_points(width, height, type) {
    var m = height < 20 ? 1 : height / 12;
    return [width - (9 * m), height * 0.5 - Math.floor(1 * m),
        width - (3 * m), height * 0.5 - Math.floor(1 * m),
        width - (6 * m), height * 0.5 + Math.floor(4 * m),
    ].join(" ");
}

function gui_atom_draw_border(cid, tag, type, width, height) {
    //post("gui_atom_draw_border type=" + type);
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var polygon = create_item(cid, "polygon", {
            points: atom_border_points(width, height, type !== 0),
            fill: "none",
            stroke: "gray",
            "stroke-width": 1,
            class: "border"
            //id: tag + "border"
        });
           
        frag.appendChild(polygon);
        if (type > 0 && type < 3) { // dropdown
            // 1 = output index
            // 2 = output value
            // Let's make the two visually distinct so that the user can still
            // reason about the patch functionality merely by reading the
            // diagram
            var m = height < 20 ? 1 : height / 12;
            var arrow = create_item(cid, "polygon", {
                points: atom_arrow_points(width, height),
                "class": type === 1 ? "arrow index_arrow" : "arrow value_arrow"
            });
            frag.appendChild(arrow);
        }
        return frag;
    });
}

function gui_atom_redraw_border(cid, tag, type, width, height) {
    //post("gui_atom_redraw_border");
    gui(cid).get_gobj(tag)
    .q("polygon",  {
        points: atom_border_points(width, height, type !== 0) 
    });
    if (type > 0 && type < 3) {
        gui(cid).get_gobj(tag)
        .q(".arrow", {
            points: atom_arrow_points(width, height)
        });
    }
}

// draw a patch cord
// ico@vt.edu: p11 added to provide different color for when the cord is
// being created vs when it is being finished
function gui_canvas_line(cid,tag,type,curved,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11) {
    gui(cid).get_elem("patchsvg")
    .append(function(frag) {
        var svg = get_item(cid, "patchsvg"),
        // xoff is for making sure straight lines are crisp.  An SVG stroke
        // straddles the coordinate, with 1/2 the width on each side.
        // Control cords are 1 px wide, which requires a 0.5 x-offset to align
        // the stroke to the pixel grid.
        // Signal cords are 2 px wide = 1px on each side-- no need for x-offset.
            xoff = type === 'signal' ? 0 : 0.5,
            d_array = (curved ?
                        ["M", p1 + xoff, p2 + xoff,
                        "Q", p3 + xoff, p4 + xoff, p5 + xoff, p6 + xoff,
                        "Q", p7 + xoff, p8 + xoff, p9 + xoff, p10 + xoff]
                        :
                        ["M", p1, p2, "L", p9, p10]
                      ),
            path;
        path = create_item(cid, "path", {
            d: d_array.join(" "),
            fill: "none",
            //"shape-rendering": "optimizeSpeed",
            id: tag, 
            "class": "cord " + type + (p11 == 1 ? " new" : "")
        });
        frag.appendChild(path);
        return frag;
    });
    // ico@vt.edu 2020-08-12: update scroll when cord is drawn
    gui_canvas_get_scroll(cid);
}

function gui_canvas_select_line(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.classList.add("selected_line");
    });
}

function gui_canvas_deselect_line(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.classList.remove("selected_line");
    });
}

// rename to erase_line (or at least standardize with gobj_erase)
function gui_canvas_delete_line(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.parentNode.removeChild(e);
    });
    // ico@vt.edu 2020-08-12: update scroll when cord is deleted
    gui_canvas_get_scroll(cid);
}

// ico@vt.edu 2021-05-03: autoscroll when patchcord exceeds the window size
// LATER: refactor this and consider combining with gui_canvas_move_selection
function gui_canvas_patchcord_scroll(cid, x1, y1, x2, y2) {
    //post("gui_canvas_patchcord_scroll");
    // need to detect in which way are we expanding the selection
    // this is used to then affect the scroll logic further below
    var old_x1, old_y1, old_x2, old_y2,
        new_x1, new_y1, new_x2, new_y2;
    new_x1 = (x1 < x2 ? x1 + 0.5 : x2 + 0.5);
    new_y1 = (y1 < y2 ? y1 + 0.5 : y2 + 0.5);
    new_x2 = (x1 > x2 ? x1 + 0.5 : x2 + 0.5);
    new_y2 = (y1 > y2 ? y1 + 0.5 : y2 + 0.5);
    gui(cid).get_elem("newcord", function(elem) {
        var old_points = elem.getAttribute("d").split(" ");
        //post("points: 0="+old_points[0]+" 1="+old_points[1]+
        //    " 2="+old_points[2]+" 7="+old_points[7]+" | "+
        //    (old_points[1]<old_points[7] ? "one" : "seven"));
        old_x1 = (parseFloat(old_points[1]) < parseFloat(old_points[11])
            ? old_points[1] : old_points[11]);
        old_y1 = (parseFloat(old_points[2]) < parseFloat(old_points[12])
            ? old_points[2] : old_points[12]);
        old_x2 = (parseFloat(old_points[1]) > parseFloat(old_points[11])
            ? old_points[1] : old_points[11]);
        old_y2 = (parseFloat(old_points[2]) > parseFloat(old_points[12])
            ? old_points[2] : old_points[12]);
    });
    //post("old: "+old_x1+" "+old_y1+" | "+old_x2+" "+old_y2);
    //post("new: "+new_x1+" "+new_y1+" | "+new_x2+" "+new_y2);
    var lr = 0, ud = 0; // lr: -1 left, 1 right, ud: -1 up, 1 down
    if (old_x2 != new_x2) lr = 1;
    if (old_x1 != new_x1) lr = -1;
    if (old_y2 != new_y2) ud = 1;
    if (old_y1 != new_y1) ud = -1;
    //post("lr="+lr+" ud="+ud);

    gui(cid).get_nw_window(function(nw_win) {
        var svg_elem = nw_win.window.document.getElementById("patchsvg");
        var { x: x, y: y, w: width, h: height,
            mw: min_width, mh: min_height } = canvas_params(nw_win, cid);
        //post("mw="+min_width+" mh="+min_height+" w="+width+" h="+height);
        // if there is nothing on the canvas, or the contents are only
        // using a subset of the canvas, we need to use window boundaries
        // as the edges of the selection area
        if (width < min_width) {
            width = min_width;
        }
        if (height < min_height) {
            height = min_height;
        }
        //post("scrollX="+nw_win.window.scrollX+
        //  " scrollY="+nw_win.window.scrollY);

        /* this is only valid for selection box, not the patch cord
           so we comment it out here
        // limit selection box size to the canvas size
        // we subtract margins to make it got one pixel away from the edge
        // except for the bottom, where we may want to add -5 because the
        // current nw.js has some unexplained dead space at the bottom
        if (x1 < x + 1) {
            x1 = x + 1;
            //post("x1 left");
        } else if (x1 > width + x - 2) {
            x1 = width + x - 1;
            //post("x1 right");
        }
        if (x2 < x + 1) {
            x2 = x + 1;
            //post("x2 left");
        } else if (x2 > width + x - 2) {
            x2 = width + x - 2;
            //post("x2 right");
        }
        if (y1 < y + 1) {
            y1 = y + 1;
            //post("y1 top");
        } else if (y1 > height + y - 2) {
            y1 = height + y - 2;
            //post("y1 bottom");
        } if (y2 < y + 1) {
            y2 = y + 1;
            //post("y2 top");
        } else if (y2 > height + y - 2) {
            y2 = height + y - 2;
            //post("y2 bottom");
        }*/

        /*
        post("x="+x+" y="+y+" raw_w="+width+" w="+(width + x - 2)+
            " raw_h="+height+" h="+(height + y - 5)+
            " | "+x1+" "+y1+" "+x2+" "+y2);
        */

        /*
        var points_array = [x1 + 0.5, y1 + 0.5, x2 + 0.5, y1 + 0.5,
            x2 + 0.5, y2 + 0.5, x1 + 0.5, y2 + 0.5];

        gui(cid).get_elem("selection_rectangle", {
            points: points_array
        });
        */
        // now check for selection outside the window boundaries and
        // whether we need to invoke the scrolling
        var gobj;
        gui(cid).get_elem("newcord", function(elem) {
            gobj = elem;
        });
        var x1b, y1b, x2b, y2b;
        var bbox = gobj.getBBox();
        x1b = bbox.x;
        y1b = bbox.y;
        x2b = x1b + bbox.width;
        y2b = y1b + bbox.height;
        //post("x1="+x1+" y1="+y1+" x2="+x2+" y2="+y2);


        var tlx, tly, brx, bry; // top-left x and y, bottom-right x and y
        var offsetx = 0, offsety = 0; // final offset

        tlx = x + nw_win.window.scrollX;
        tly = y + nw_win.window.scrollY;
        brx = tlx + min_width;
        bry = tly + min_height;
        //post("("+tlx+","+tly+") | ("+brx+","+bry+")");
        /*
        // old way
        if (lr == 1) {
            if (x2b - brx > 0)
                offsetx = x2b - brx;
            else if (x2 - tlx < 0)
                offsetx = x2b - tlx;
        } else if (lr == -1) {
            if (x1b - tlx < 0)
                offsetx = x1b - tlx;
            else if (x1 - brx > 0)
                offsetx = x1b - brx;
        }

        if (ud == 1) {
            if (y2b - bry > 0)
                offsety = y2b - bry;
            else if (y2b - tly < 0)
                offsety = y2b - tly;
        } else if (ud == -1) {
            if (y1b - tly < 0)
                offsety = y1b - tly;
            else if (y1b - bry > 0)
                offsety = y1b - bry;
        }
        */
        // ico@vt.edu 2021-10-19: added edge zone where scroll is initiated
        // before even getting to the edge of the canvas because it appears
        // the current nw.js version sometimes stops reporting mouse position
        // when the mouse is rapidly pulled away from the window, which
        // results in the window not being scrollable anymore until the
        // mouse button is released and the scroll is reinitialized.
        var edgezone = 30;
        
        if (lr == 1) {
            if (x2b - (brx-edgezone) > 0) {
                offsetx = x2b - (brx-edgezone);
                //post("lr 1 1: " + (x2b - (brx-edgezone)));
            }
            else if ((x2b-edgezone) - tlx < 0) {
                offsetx = (x2b-edgezone) - tlx;
                //post("lr 1 2: " + ((x2b-edgezone) - tlx));
            }
        } else if (lr == -1) {
            if ((x1b-edgezone) - tlx < 0) {
                offsetx = (x1b-edgezone) - tlx;
                //post("lr -1 2: " + ((x1b-edgezone) - tlx));
            }
            else if (x1 - (brx-edgezone) > 0) {
                offsetx = x1b - (brx-edgezone);
                //post("lr -1 2: " + (x1b - (brx-edgezone)));
            }
        }

        if (ud == 1) {
            if (y2b - (bry-edgezone) > 0) {
                offsety = y2b - (bry-edgezone);
                //post("ud 1 1: " + (y2b - (bry-edgezone)));
            }
            else if ((y2b-edgezone) - tly < 0) {
                offsety = (y2b-edgezone) - tly;
                //post("ud 1 2: " + ((y2b-edgezone) - tly));
            }
        } else if (ud == -1) {
            if ((y1b-edgezone) - tly < 0) {
                offsety = (y1b-edgezone) - tly;
                //post("ud -1 1: " + ((y1b-20) - tly));
            }
            else if (y1b - (bry-edgezone) > 0) {
                offsety = y1b - (bry-edgezone);
                //post("ud -1 2: " + (y1b - (bry-20)));
            }
        }


        var inc = 10; // scroll increment
        if (offsetx > 0) offsetx = inc;
        else if (offsetx < 0) offsetx = -inc;
        if (offsety > 0) offsety = inc;
        else if (offsety < 0) offsety = -inc;
        /*
        if (x2 - brx > 0)
            offsetx = x2 - brx;
        else if (x1 - tlx < 0)
            offsetx = x1 - tlx;

        if (y2 - bry > 0)
            offsety = y2 - bry;
        else if (y1 - tly < 0)
            offsety = y1 - tly;
        */

        //post("final x:"+offsetx+" y:"+offsety);
        if (offsetx != 0 || offsety != 0) {
            nw_win.window.scrollBy({
                //left: (offsetx > 0 ? 10 : -10),
                //top: (offsety > 0 ? 10 : -10),
                left: offsetx,
                top: offsety,
                behavior: 'auto'
            });
        }
    });
}

function gui_canvas_update_line(cid, tag, curved, x1, y1, x2, y2, yoff) {
    // We have to check for existence here for the special case of
    // preset_node which hides a wire that feeds back from the downstream
    // object to its inlet. Pd refrains from drawing this hidden wire at all.
    // It should also suppress a call here to update that line, but it
    // currently doesn't. So we check for existence.
    gui(cid).get_elem(tag, function(e) {
        var halfx = parseInt((x2 - x1)/2),
            halfy = parseInt((y2 - y1)/2),
            xoff, // see comment in gui_canvas_line about xoff
            d_array;
        xoff = e.classList.contains("signal") ? 0: 0.5;
        
        d_array = (curved ?
                    ["M",x1+xoff,y1+xoff,
                    "Q",x1+xoff,y1+yoff+xoff,x1+halfx+xoff,y1+halfy+xoff,
                    "Q",x2+xoff,y2-yoff+xoff,x2+xoff,y2+xoff]
                    :
                    ["M", x1, y1, "L", x2, y2]
                  );
        // ico@vt.edu 2021-05-03:
        // if we are a new cord created by a user, we will have a tag
        // "newcord", as reflected in g_editor.c, so we need to check
        // if we need to autoscroll when the patch cord exceeds the
        // window size and window is scrollable. Here, we don't worry
        // about updating patch cord and getting bbox because patch cords
        // do not enlarge the canvas if the mouse goes outside the canvas'
        // current bbox.
        if (tag === "newcord") {
            gui_canvas_patchcord_scroll(cid, x1, y1, x2, y2);
        }
        configure_item(e, { d: d_array.join(" ") });
    });
}

function text_line_height_kludge(fontsize, fontsize_type) {
    var pd_fontsize = fontsize_type === "gui" ?
        gui_fontsize_to_pd_fontsize(fontsize) :
        fontsize;
    //post("text_line_height_kludge fontsize=" + fontsize);
    switch (pd_fontsize) {
        case 8: return 11;
        case 10: return 13;
        case 12: return 16;
        case 16: return 19;
        case 24: return 29;
        case 36: return 44;
        default: return fontsize + 2;
    }
}

function text_to_tspans(cid, svg_text, text, type) {
    var lines, i, isatom, len, tspan, fontsize, newtext, text_node;
    // the type argument is optional, but it *will* be set when we're being
    // called via rtext_senditup() in order to update an object text
    var is_comment = type && type === "text";
    var init_attr, style = {}, fill = null;
    // Get fontsize (minus the trailing "px")
    fontsize = svg_text.getAttribute("font-size")
    if (fontsize.endsWith('px'))
        fontsize = fontsize.slice(0, -2);
    var dy = text_line_height_kludge(+fontsize, "gui");
    
    function make_tspan(span) {
        // remove escaped commas and semicolons in gatom and comment objects only
        newtext = null;
        if (svg_text.classList.contains("atom")) {
            //post("text_to_tspans " + 
            //    (type.contains("comment") ? "comment" : "other") + " escaping");
            isatom = 1;
            //newtext = text.replace(/\n/g, '');
            newtext = text.replace(/\\/g,'');
            //post("atom newtext=<" + newtext +">");
            // ico@vt.edu 2021-04-16:
            // now truncate gatom by adding '>' at the end of overflow
            // we do this here because it is more flexible and efficient than
            // in c, since this is done post-escaping the \\s
            var parentWidth;
            // if we have no parent, that means we are resizing the gatom using
            // mouse which forces recreation of the text from c using
            // gui_text_new and since we are appended to the parent only after
            // text_to_tspans (see the end of the gui_text_new call), we have
            // no parent and have to do this ugly workaround to find our parent.
            // if, on the other hand we have a parent, then we are likely editing
            // the contents of the gatom and therefore we can simply look for
            // our parent. either way, we need to truncate text, so that it does

            if (svg_text.parentNode === null)
            {
                parentWidth = patchwin[cid].window.document.getElementById(
                    svg_text.id.replace(/text/g,'gobj')).getBBox().width;
            }
            else
                parentWidth = svg_text.parentNode.getBBox().width;

            //post("contains data newtext=" + newtext + " " + parentWidth);
            var width = Math.floor(
                parentWidth / svg_text.getAttribute("font-width"));
            //post(newtext.length + " " + width);
            if (newtext.length > width) {
                //post("old newtext=<"+newtext+">");
                newtext = newtext.substr(0, width-1) + '>';
                //post("new newtext=<"+newtext+">");
            }
        }
        if(newtext == null || newtext?.length == 0) {
            if (span === '')
                span = ' ';
            newtext = span;
        }

        if (is_comment) {
            // tags can be escaped with <!tag>, show these as literals
            newtext = newtext.replace(/<!([!=a-z0-9#/]+)>/g, "<$1>");
        }
        if (newtext.length > 0) {
            // find a way to abstract away the canvas array and the DOM here
            var attr = init_attr;
            if (fill) attr.fill = fill;
            tspan = create_item(cid, "tspan", attr);
            // make sure that spaces properly show
            tspan.style.setProperty("white-space", "pre");
            for (var x in style) tspan.style[x] = style[x];
            text_node = patchwin[cid].window.document
                .createTextNode(newtext);
            if (!/^\s*$/.test(newtext)) init_attr = {};
            tspan.appendChild(text_node);
            svg_text.appendChild(tspan);
        }
    }

    isatom = 0;
    lines = text.split("\n");
    len = lines.length;
    for (i = 0; i < len; i++) {
        var spans = [lines[i]], tags = null;
        init_attr = { dy: i==0 ? 0 : dy + "px", x: 0 };
        delete style.visibility;
        if (/^\s*$/.test(lines[i])) {
            // ag: empty spans won't render correctly, so add something other
            // than a blank and hide the contents of the tspan. See
            // https://stackoverflow.com/a/58429593
            style.visibility = "hidden";
            spans[0] = ".";
        } else if (is_comment) {
            // split the text into individual spans and tags
            spans = lines[i].split(/<\/?(?:[bhisu]|=[a-z0-9#]+)>/);
            tags = lines[i].match(/<\/?([bhisu]|=[a-z0-9#]+)>/g);
        }
        make_tspan(spans[0]);
        if (tags) {
            for (var j = 1, k = 0; j < spans.length; j++, k++) {
                var tag = tags[k];
                switch (tag) {
                case "<b>":
                    style.fontWeight = "bold";
                    break;
                case "</b>":
                    delete style.fontWeight;
                    break;
                case "<i>":
                    style.fontStyle = "italic";
                    break;
                case "</i>":
                    delete style.fontStyle;
                    break;
                case "<s>":
                    style.textDecoration = "line-through";
                    break;
                case "</s>":
                    delete style.textDecoration;
                    break;
                case "<u>":
                    style.textDecoration = "underline";
                    break;
                case "</u>":
                    delete style.textDecoration;
                    break;
                case "<h>":
                    style.fontSize = "120%";
                    style.fontWeight = "bold";
                    break;
                case "</h>":
                    delete style.fontSize;
                    delete style.fontWeight;
                    break;
                default:
                    // any tag starting with '=' is taken to be a color
                    if (tag.startsWith("<=")) {
                        fill = tag.slice(2, -1);
                    } else if (tag.startsWith("</=") && tag.slice(3, -1) === fill) {
                        fill = null;
                    }
                    break;
                }
                make_tspan(spans[j]);
            }
        }
        if (isatom)
            break;
    }
}

// To keep the object and message box size consistent
// with Pd-Vanilla, we make small changes to the font
// sizes before rendering. If this impedes readability
// we can revisit the issue. Even Pd-Vanilla's box sizing
// changed at version 0.43, so we can break as well if
// it comes to that.

function font_map() {
    return {
        // pd_size: gui_size
        8: 8.33,
        12: 11.65,
        16: 16.65,
        24: 23.3,
        36: 36.6
    };
}

// This is a suboptimal font map, necessary because some genius "improved"
// the font stack on Gnu/Linux by delivering font metrics that don't match
// at all with what you get in OSX, Windows, nor even the previous version
// of the Gnu/Linux stack.
function suboptimal_font_map() {
    return {
        // pd_size: gui_size
        8: 8.45,
        12: 11.4,
        16: 16.45,
        24: 23.3,
        36: 36
    }
}

function gobj_fontsize_kludge(fontsize, return_type) {
    // These were tested on an X60 running Trisquel (based
    // on Ubuntu 14.04)
    var ret, prop,
        fmap = font_stack_is_maintained_by_troglodytes() ?
            suboptimal_font_map() : font_map();
    if (return_type === "gui") {
        ret = fmap[fontsize];
        return ret ? ret : fontsize;
    } else {
        for (prop in fmap) {
            if (fmap.hasOwnProperty(prop)) {
                if (fmap[prop] == fontsize) {
                    return +prop;
                }
            }
        }
        return fontsize;
    }
}

function pd_fontsize_to_gui_fontsize(fontsize) {
    return gobj_fontsize_kludge(fontsize, "gui");
}

function gui_fontsize_to_pd_fontsize(fontsize) {
    return gobj_fontsize_kludge(fontsize, "pd");
}

// Another hack, similar to above. We use this to
// make sure that there is enough vertical space
// between lines to fill the box when there is
// multi-line text.
function gobj_font_y_kludge(fontsize) {
    switch (fontsize) {
        case 8: return -0.5;
        case 10: return -1;
        case 12: return -1;
        case 16: return -1.5;
        case 24: return -3;
        case 36: return -6;
        default: return 0;
    }
}

function gui_text_new(cid, tag, type, isselected, left_margin,
    font_height, text, font, font_width, is_toplevel) {
    //ico@vt.edu: different text spacing for GOPs
    //post("gui_text_new type=" + type + " tag=" + tag + " text=<" + text + ">");
    var xoff = 0.5; // Default value for normal objects, GOP uses -0.5
    var yoff = 0;
    /* ico@vt.edu 20200907: the following id_suffix is used for gatom objects.
    When activated, they tend to highlight both the label and the gatom contents
    since prior to this there was no differentiation between the two in terms of
    their tags. However, g_rtext.c instantiates gatom contents with type "atom"
    whereas the label inside g_text.c is instantiated as "gatom". We use this
    difference here to provide the two with different tag names, so that we can
    prevent the label from being also "activated" (e.g. when user clicks on the
    gatom to edit its contents in non-edit mode). */

    if (type === "text") {
        // if we are comment, remove escaped stuf
        // we need to do this here because text_to_tspans will not know its parent
        // and therefore the escaping there will not have any effect when called
        // below
        //text = text.replace(/\\,/g,",");
        //text = text.replace(/\\;/g,";");
    }

    var classname = (type === "broken" ? "broken " : "") + "box_text";
    if (type === "atom") {
        classname = "cut copy paste box_text data atom";
    }
    if (is_toplevel === 1) {
        classname = classname + " toplevel";
    }
    if (type === "graph_label") {
        xoff -= 2;
    }
    /*
    gui(cid).get_gobj(tag, function(e) {
        if(e.classList.contains("graph")) {
            //xoff = -1;
            //yoff = 1;
        }
    });
    */

/*
    // TODO: eElement.insertBefore(newFirstElement, eElement.firstChild);
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var svg_text = create_item(cid, "text", {
            // Maybe it's just me, but the svg spec's explanation of how
            // text x/y and tspan x/y interact is difficult to understand.
            // So here we just translate by the right amount for the
            // left-margin, guaranteeing all tspan children will line up where
            // they should be.

            // Another anomaly-- we add 0.5 to the translation so that the font
            // hinting works correctly. This effectively cancels out the 0.5
            // pixel alignment done in the gobj, so it might be better to
            // specify the offset in whatever is calling this function.

            // I don't know how svg text grid alignment relates to other svg
            // shapes, and I haven't yet found any documentation for it. All I
            // know is an integer offset results in blurry text, and the 0.5
            // offset doesn't.
            transform: "translate(" + (left_margin - xoff) + "," + yoff + ")",
            y: font_height - 0.5 + gobj_font_y_kludge(font),
            // Turns out we can't do 'hanging' baseline
            // because it's borked when scaled. Bummer, because that's how Pd's
            // text is handled under tk...
            // 'dominant-baseline': 'hanging',
            "shape-rendering": "crispEdges",
            "font-width": font_width,
            "font-size": pd_fontsize_to_gui_fontsize(font) + "px",
            "font-weight": "normal",
            id: tag + "text",
            "class": classname
        });
        // trim off any extraneous leading/trailing whitespace. Because of
        // the way binbuf_gettext works we almost always have a trailing
        // whitespace.
        text = text.trim();
        // fill svg_text with tspan content by splitting on '\n'
        text_to_tspans(cid, svg_text, text);
        frag.appendChild(svg_text);
        if (isselected) {
            gui_gobj_select(cid, tag);
        }
        return frag;
    });
*/

    gui(cid).get_gobj(tag, function(parent) {
        var svg_text = create_item(cid, "text", {
            // Maybe it's just me, but the svg spec's explanation of how
            // text x/y and tspan x/y interact is difficult to understand.
            // So here we just translate by the right amount for the
            // left-margin, guaranteeing all tspan children will line up where
            // they should be.

            // Another anomaly-- we add 0.5 to the translation so that the font
            // hinting works correctly. This effectively cancels out the 0.5
            // pixel alignment done in the gobj, so it might be better to
            // specify the offset in whatever is calling this function.

            // I don't know how svg text grid alignment relates to other svg
            // shapes, and I haven't yet found any documentation for it. All I
            // know is an integer offset results in blurry text, and the 0.5
            // offset doesn't.
            transform: "translate(" + (left_margin - xoff) + "," + yoff + ")",
            y: font_height - 0.5 + gobj_font_y_kludge(font),
            // Turns out we can't do 'hanging' baseline
            // because it's borked when scaled. Bummer, because that's how Pd's
            // text is handled under tk...
            // 'dominant-baseline': 'hanging',
            "shape-rendering": "crispEdges",
            "font-width": font_width,
            "font-size": pd_fontsize_to_gui_fontsize(font) + "px",
            "font-weight": "normal",
            id: tag + "text",
            "class": classname + (type === "graph_label" ? " graph_label" : "")
        });
        // trim off any extraneous leading/trailing whitespace. Because of
        // the way binbuf_gettext works we almost always have a trailing
        // whitespace.
        text = text.trim();
        // fill svg_text with tspan content by splitting on '\n'
        text_to_tspans(cid, svg_text, text, type);
        if (is_toplevel && parent.classList.contains("gop"))
            parent.prepend(svg_text);
        else
            parent.appendChild(svg_text);
        // ico 2023-08-20: ugly workaround for RPi's outdated nwjs
        /*
        gui(cid).get_gobj(tag, function(e) {
            if(type === "graph_label" && !check_nw_version("0.67")) {
                //post("got graph label on outdated nwjs");
                //svg_text.style.setProperty("mix-blend-mode", "inherit");
                //svg_text.style.setProperty("font-weight", "normal");
            }
        });
        */
        if (isselected) {
            gui_gobj_select(cid, tag);
        }
    });
}

// Because of the overly complex code path inside
// canvas_setgraph, multiple erasures can be triggered in a row.
function gui_gobj_erase(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_text_set (cid, tag, text, type) {
    //post("gui_text_set tag=" + tag + " text=<" + text + ">");
    gui(cid).get_elem(tag + "text", function(e) {
        text = text.trim();
        e.textContent = "";
        text_to_tspans(cid, e, text, type);
    });
}

function gui_text_set_mynumbox (cid, tag, text, active) {
    gui(cid).get_elem(tag + "text", function(e) {
        text = text.trim();
        e.textContent = "";
        text_to_tspans(cid, e, text);
        if (active === 1) {
            e.classList.add("activated");
        } else {
            e.classList.remove("activated");           
        }
    });
}

function gui_text_redraw_border(cid, tag, width, height) {
    // Hm, need to figure out how to refactor to get rid of
    // configure_item call...
    gui(cid).get_gobj(tag, function(e) {
        var b = e.querySelectorAll(".border"),
        i;
        for (i = 0; i < b.length; b++) {
            configure_item(b[i], {
                width: width,
                height: height
            });
        }
    });
}

function gui_gobj_select(cid, tag) {
    //post("gui_gobj_select tag=" + tag);
    gui(cid).get_gobj(tag, function(e, w) {
        //var i, all_elems; // all GOP elements retrievable by the shared class name
        /*if (e.classList.contains("graph") && e.classList.length > 1) {
           all_elems = w.document.getElementsByClassName(e.classList[1]);
            for (i = 0; i < all_elems.length; i++) {
                all_elems[i].classList.add("selected");
            }
        } else {*/
            e.classList.add("selected");
        //}
    });
}

function gui_gobj_deselect(cid, tag) {
    gui(cid).get_gobj(tag, function(e, w) {
        /*
        var i, all_elems; // all GOP elements retrievable by the shared class name
        if (e.classList.contains("graph") && e.classList.length > 1) {
           all_elems = w.document.getElementsByClassName(e.classList[1]);
            for (i = 0; i < all_elems.length; i++) {
                all_elems[i].classList.remove("selected");
            }
        } else {
        */
        e.classList.remove("selected");
        //}
        // ico@vt.edu: check for scroll in case the handle disappears
        // during deselect. LATER: make handles always fit inside the
        // object, so this won't be necessary
        gui_canvas_get_scroll(cid);
    });
}

function gui_gobj_dirty(cid, tag, state) {
    //post("gui_gobj_dirty cid=" + cid + " state=" + state);
    gui(cid).get_gobj(tag, function(e) {
        // ico@vt.edu 2022-09-28: we add two conditions, one
        // for border classes (canvas with GOP disabled) used
        // by non-GOP abstractions, and another, gopborder,
        // for GOP-enabled abstractions
        var border = e.querySelectorAll(":scope > .gopborder");
        if (border.length > 0) {
            //post("...got gopborder length=" + border.length + " gopborder=" + border);
            border[0].classList.remove("dirty");
            border[0].classList.remove("subdirty");
            if(state === 1) border[border.length - 1].classList.add("dirty");
            else if(state === 2) border[border.length - 1].classList.add("subdirty");
        }
        border = e.querySelectorAll(":scope > .border");
        if (border.length > 0) {
            //post("...got border length=" + border.length + " border=" + border);
            border[0].classList.remove("dirty");
            border[0].classList.remove("subdirty");
            if(state === 1) border[border.length - 1].classList.add("dirty");
            else if(state === 2) border[border.length - 1].classList.add("subdirty");
        }
    });
}

function gui_canvas_warning(cid, warid) {
    var warning = get_item(cid, "canvas_warning");
    switch(warid)
    {
        case 0:
            warning.style.setProperty("display", "none");
            break;
        case 1:
            warning.title = lang.get_local_string("canvas.warning.unsaved_tt");
            warning.onclick = function(){ pdsend(cid, "showdirty"); }
            warning.style.setProperty("color", "coral");
            warning.style.setProperty("font-size", "x-large");
            warning.style.setProperty("display", "inline");
            break;
        case 2:
            warning.title = lang.get_local_string("canvas.warning.multipleunsaved_tt");
            warning.onclick = function(){ pdsend(cid, "showdirty"); }
            warning.style.setProperty("color", "red");
            warning.style.setProperty("font-size", "xx-large");
            warning.style.setProperty("display", "inline");
            break;

        default:
            break;
    }
}

exports.gui_canvas_warning = gui_canvas_warning;

function gui_canvas_emphasize(cid) {
    gui(cid).get_elem("patchsvg", function(e) {
        // raise the window
        gui_raise_window(cid);
        // animate the background, except for Windows and old OSX versions.
        // We *really* have to update all platforms to the same most recent
        // stable version. Otherwise the entire codebase is going to become
        // conditional branches...
        if (check_nw_version("0.15")) {
            e.animate([
                {"backgroundColor": "white"},
                {"backgroundColor": "#ff9999"},
                {"backgroundColor": "white"}
            ], { duration: 900, easing: "ease-in-out", iterations: 1 });
        }
    });
}

// bring a gobj into the viewport, plus do an animation to catch the
// user's attention
function gui_gobj_emphasize(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        var border = e.querySelector(".border");
        e.scrollIntoView();
        // quick and dirty, plus another check because Windows and old OSX
        // versions of nwjs are ancient and don't include web animations API...
        if (border && check_nw_version("0.15")) {
            border.animate([
                 {fill: "white"},
                 {fill: "#ff9999"},
                 {fill: "white"}
            ], { duration: 300, easing: "ease-in-out", iterations: 3});
        }
    });
}

// This adds a 0.5 offset to align to pixel grid, so it should
// only be used to move gobjs to a new position.  (Should probably
// be renamed to gobj_move to make this more obvious.)
function elem_move(elem, x, y) {
    var t = elem.transform.baseVal.getItem(0);
    t.matrix.e = x+0.5;
    t.matrix.f = y+0.5;
}

function elem_displace(elem, dx, dy) {
        var t = elem.transform.baseVal.getItem(0);
        t.matrix.e += dx;
        t.matrix.f += dy;
}

function elem_get_coords(elem) {
    var t = elem.transform.baseVal.getItem(0);
    return {
        x: t.matrix.e,
        y: t.matrix.f
    }
}

// used for tidy up and GUI external displacefn callbacks
function gui_text_displace(name, tag, dx, dy) {
    gui(name).get_gobj(tag, function(e) {
        elem_displace(e, dx, dy);
    });
}

function textentry_displace(t, dx, dy) {
    var transform = t.style.getPropertyValue("transform")
        .split("(")[1]    // get everything after the "("
        .replace(")", "") // remove trailing ")"
        .split(",");      // split into x and y
    var x = +transform[0].trim().replace("px", ""),
        y = +transform[1].trim().replace("px", "");
    t.style.setProperty("transform",
        "translate(" +
        (x + dx) + "px, " +
        (y + dy) + "px)");
}

function gui_canvas_displace_withtag(cid, dx, dy) {
    gui(cid)
    .get_elem("patchsvg", function(svg_elem, w) {
        var i, ol;
        ol = w.document.getElementsByClassName("selected");
        for (i = 0; i < ol.length; i++) {
            /*if (ol[i].classList.contains("graph")) {
                ol[i].setAttribute('x', parseFloat(ol[i].getAttribute('x')) + dx);
                ol[i].setAttribute('y', parseFloat(ol[i].getAttribute('y')) + dy);
            } else {*/
                elem_displace(ol[i], dx, dy);
            //}
        }
    })
    .get_elem("new_object_textentry", function(textentry) {
        textentry_displace(textentry, dx, dy);
    });
}

function gui_canvas_draw_selection(cid, x1, y1, x2, y2) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        var points_array = [x1 + 0.5, y1 + 0.5,
                            x2 + 0.5, y1 + 0.5,
                            x2 + 0.5, y2 + 0.5,
                            x1 + 0.5, y2 + 0.5
        ];
        var rect = create_item(cid, "polygon", {
            points: points_array.join(" "),
            fill: "none",
            //"shape-rendering": "optimizeSpeed",
            "stroke-width": 1,
            id: "selection_rectangle",
            display: "inline"
        });
        svg_elem.appendChild(rect);
    });
}

function gui_canvas_move_selection(cid, x1, y1, x2, y2) {
    //post("gui_canvas_move_selection");
    // need to detect in which way are we expanding the selection
    // this is used to then affect the scroll logic further below
    var old_x1, old_y1, old_x2, old_y2,
        new_x1, new_y1, new_x2, new_y2;
    new_x1 = (x1 < x2 ? x1 + 0.5 : x2 + 0.5);
    new_y1 = (y1 < y2 ? y1 + 0.5 : y2 + 0.5);
    new_x2 = (x1 > x2 ? x1 + 0.5 : x2 + 0.5);
    new_y2 = (y1 > y2 ? y1 + 0.5 : y2 + 0.5);
    gui(cid).get_elem("selection_rectangle", function(elem) {
        var old_points = elem.getAttribute("points").split(",");
        //post("points: 0="+old_points[0]+" 1="+old_points[1]+
        //    " 2="+old_points[2]+" 7="+old_points[7]+" | "+
        //    (old_points[1]<old_points[7] ? "one" : "seven"));
        old_x1 = (parseFloat(old_points[0]) < parseFloat(old_points[2])
            ? old_points[0] : old_points[2]);
        old_y1 = (parseFloat(old_points[1]) < parseFloat(old_points[7])
            ? old_points[1] : old_points[7]);
        old_x2 = (parseFloat(old_points[0]) > parseFloat(old_points[2])
            ? old_points[0] : old_points[2]);
        old_y2 = (parseFloat(old_points[1]) > parseFloat(old_points[7])
            ? old_points[1] : old_points[7]);
    });
    //post("old: "+old_x1+" "+old_y1+" | "+old_x2+" "+old_y2);
    //post("new: "+new_x1+" "+new_y1+" | "+new_x2+" "+new_y2);
    var lr = 0, ud = 0; // lr: -1 left, 1 right, ud: -1 up, 1 down
    if (old_x2 != new_x2) lr = 1;
    if (old_x1 != new_x1) lr = -1;
    if (old_y2 != new_y2) ud = 1;
    if (old_y1 != new_y1) ud = -1;
    //post("lr="+lr+" ud="+ud);

    gui(cid).get_nw_window(function(nw_win) {
        var svg_elem = nw_win.window.document.getElementById("patchsvg");
        var { x: x, y: y, w: width, h: height,
            mw: min_width, mh: min_height } = canvas_params(nw_win, cid);
        //post("mw="+min_width+" mh="+min_height+" w="+width+" h="+height);
        // if there is nothing on the canvas, or the contents are only
        // using a subset of the canvas, we need to use window boundaries
        // as the edges of the selection area
        if (width < min_width) {
            width = min_width;
        }
        if (height < min_height) {
            height = min_height;
        }
        //post("scrollX="+nw_win.window.scrollX+
        //  " scrollY="+nw_win.window.scrollY);

        // limit selection box size to the canvas size
        // we subtract margins to make it got one pixel away from the edge
        // except for the bottom, where we may want to add -5 because the
        // current nw.js has some unexplained dead space at the bottom
        if (x1 < x + 1) {
            x1 = x + 1;
            //post("x1 left");
        } else if (x1 > width + x - 2) {
            x1 = width + x - 1;
            //post("x1 right");
        }
        if (x2 < x + 1) {
            x2 = x + 1;
            //post("x2 left");
        } else if (x2 > width + x - 2) {
            x2 = width + x - 2;
            //post("x2 right");
        }
        if (y1 < y + 1) {
            y1 = y + 1;
            //post("y1 top");
        } else if (y1 > height + y - 2) {
            y1 = height + y - 2;
            //post("y1 bottom");
        } if (y2 < y + 1) {
            y2 = y + 1;
            //post("y2 top");
        } else if (y2 > height + y - 2) {
            y2 = height + y - 2;
            //post("y2 bottom");
        }

        /*
        post("x="+x+" y="+y+" raw_w="+width+" w="+(width + x - 2)+
            " raw_h="+height+" h="+(height + y - 5)+
            " | "+x1+" "+y1+" "+x2+" "+y2);
        */

        var points_array = [x1 + 0.5, y1 + 0.5, x2 + 0.5, y1 + 0.5,
            x2 + 0.5, y2 + 0.5, x1 + 0.5, y2 + 0.5];

        //post("points_array=" + points_array);
        gui(cid).get_elem("selection_rectangle", {
            points: points_array
        });
        // now check for selection outside the window boundaries and
        // whether we need to invoke the scrolling
        var gobj;
        gui(cid).get_elem("selection_rectangle", function(elem) {
            gobj = elem;
        });
        var x1b, y1b, x2b, y2b;
        var bbox = gobj.getBBox();
        x1b = bbox.x;
        y1b = bbox.y;
        x2b = x1b + bbox.width;
        y2b = y1b + bbox.height;
        //post("x1="+x1+" y1="+y1+" x2="+x2+" y2="+y2);


        var tlx, tly, brx, bry; // top-left x and y, bottom-right x and y
        var offsetx = 0, offsety = 0; // final offset

        tlx = x + nw_win.window.scrollX;
        tly = y + nw_win.window.scrollY;
        brx = tlx + min_width;
        bry = tly + min_height;
        //post("("+tlx+","+tly+") | ("+brx+","+bry+")");

        // ico@vt.edu 2021-10-19: added edge zone where scroll is initiated
        // before even getting to the edge of the canvas because it appears
        // the current nw.js version sometimes stops reporting mouse position
        // when the mouse is rapidly pulled away from the window, which
        // results in the window not being scrollable anymore until the
        // mouse button is released and the scroll is reinitialized.
        var edgezone = 30;

        if (lr == 1) {
            if (x2b - (brx-edgezone) > 0) {
                offsetx = x2b - (brx-edgezone);
                //post("lr 1 1: " + (x2b - (brx-edgezone)));
            }
            else if ((x2b-edgezone) - tlx < 0) {
                offsetx = (x2b-edgezone) - tlx;
                //post("lr 1 2: " + ((x2b-edgezone) - tlx));
            }
        } else if (lr == -1) {
            if ((x1b-edgezone) - tlx < 0) {
                offsetx = (x1b-edgezone) - tlx;
                //post("lr -1 2: " + ((x1b-edgezone) - tlx));
            }
            else if (x1 - (brx-edgezone) > 0) {
                offsetx = x1b - (brx-edgezone);
                //post("lr -1 2: " + (x1b - (brx-edgezone)));
            }
        }

        if (ud == 1) {
            if (y2b - (bry-edgezone) > 0) {
                offsety = y2b - (bry-edgezone);
                //post("ud 1 1: " + (y2b - (bry-edgezone)));
            }
            else if ((y2b-edgezone) - tly < 0) {
                offsety = (y2b-edgezone) - tly;
                //post("ud 1 2: " + ((y2b-edgezone) - tly));
            }
        } else if (ud == -1) {
            if ((y1b-edgezone) - tly < 0) {
                offsety = (y1b-edgezone) - tly;
                //post("ud -1 1: " + ((y1b-20) - tly));
            }
            else if (y1b - (bry-edgezone) > 0) {
                offsety = y1b - (bry-edgezone);
                //post("ud -1 2: " + (y1b - (bry-20)));
            }
        }

        var inc = 10; // scroll increment
        if (offsetx > 0) offsetx = inc;
        else if (offsetx < 0) offsetx = -inc;
        if (offsety > 0) offsety = inc;
        else if (offsety < 0) offsety = -inc;
        /*
        if (x2 - brx > 0)
            offsetx = x2 - brx;
        else if (x1 - tlx < 0)
            offsetx = x1 - tlx;

        if (y2 - bry > 0)
            offsety = y2 - bry;
        else if (y1 - tly < 0)
            offsety = y1 - tly;
        */

        //post("final x:"+offsetx+" y:"+offsety);
        if (offsetx != 0 || offsety != 0) {
            nw_win.window.scrollBy({
                //left: (offsetx > 0 ? 10 : -10),
                //top: (offsety > 0 ? 10 : -10),
                left: offsetx,
                top: offsety,
                behavior: 'auto'
            });
        }
    });
}

function gui_canvas_hide_selection(cid) {
    gui(cid).get_elem("selection_rectangle", function(e) {
        e.parentElement.removeChild(e);
    });
}

// iemguis

function gui_iemgui_css(cid, tag, elem, prop, args) {
    gui(cid).get_elem(tag+elem, function(item) {
        item.style.setProperty(prop, args.join(' '));
        //item.setAttribute(prop, args.join(' '));
    });
}

function gui_bng_new(cid, tag, cx, cy, radius) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var circle = create_item(cid, "circle", {
            cx: cx,
            cy: cy,
            r: radius,
            "shape-rendering": "auto",
            fill: "none",
            stroke: "black",
            "stroke-width": 1,
            id: tag + "button"
        });
        frag.appendChild(circle);
        return frag;
    });
}

function gui_bng_button_color(cid, tag, color) {
    gui(cid).get_elem(tag + "button", {
        fill: color
    });
}

function gui_bng_configure(cid, tag, color, cx, cy, r) {
    gui(cid).get_elem(tag + "button", {
        cx: cx,
        cy: cy,
        r: r,
        fill: color
    });
}

function gui_toggle_new(cid, tag, color, width, state, p1,p2,p3,p4,p5,p6,p7,p8,basex,basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var points = [p1 - basex, p2 - basey,
                      p3 - basex, p4 - basey
        ].join(" ");
        var cross1 = create_item(cid, "polyline", {
            points: points,
            stroke: color,
            fill: "none",
            id: tag + "cross1",
            display: state ? "inline" : "none",
            "stroke-width": width
        });
        points = [p5 - basex, p6 - basey,
                  p7 - basex, p8 - basey
        ].join(" ");
        var cross2 = create_item(cid, "polyline", {
            points: points,
            stroke: color,
            fill: "none",
            id: tag + "cross2",
            display: state ? "inline" : "none",
            "stroke-width": width
        });
        frag.appendChild(cross1);
        frag.appendChild(cross2);
        return frag;
    });
}

function gui_toggle_resize_cross(cid,tag,w,p1,p2,p3,p4,p5,p6,p7,p8,basex,basey) {
    var points1 = [p1 - basex, p2 - basey,
                  p3 - basex, p4 - basey
    ].join(" "),
        points2 = [p5 - basex, p6 - basey,
                        p7 - basex, p8 - basey
    ].join(" ");
    gui(cid)
    .get_elem(tag + "cross1", {
        points: points1,
        "stroke-width": w
    })
    .get_elem(tag + "cross2", {
        points: points2,
        "stroke-width": w
    });
}

function gui_toggle_update(cid, tag, state, color) {
    var disp = !!state ? "inline" : "none";
    gui(cid)
    .get_elem(tag + "cross1", {
        display: disp,
        stroke: color
    })
    .get_elem(tag + "cross2", {
        display: disp,
        stroke: color
    })
}

function numbox_data_string_frame(w, h) {
    return ["M", 0, 0,
            "L", w - 4, 0,
                 w, 4,
                 w, h,
                 0, h,
            "z"]
    .join(" ");
}

function numbox_data_string_triangle(w, h) {
    return ["M", 0, 0,
            "L", 0, 0,
                 (h / 2)|0, (h / 2)|0, // |0 to force int
                 0, h]
    .join(" ");
}

// TODO: send fewer parameters from c (ico@vt.edu 20200916: not sure if this is possible)
function gui_numbox_new(cid, ownercid, parentcid, tag, color, x, y, w, h, drawstyle, is_toplevel) {
    // numbox doesn't have a standard iemgui border,
    // so we must create its gobj manually
    //gui(cid).get_elem("patchsvg", function() {
    //    var g = gui_gobj_new(cid, tag, "iemgui", x, y, is_toplevel);
    var g = gui_gobj_new(cid, ownercid, parentcid, tag, "iemgui", x, y, is_toplevel, 0, "numbox2", "numbox2");
    var border = create_item(cid, "path", {
        d: numbox_data_string_frame(w, h),
        fill: color,
        stroke: "black",
        "stroke-width": (drawstyle < 2 ? 1 : 0),
        id: (tag + "border"),
        "class": (is_toplevel === 1 ? "toplevel " : "") + "border"
    });
    g.appendChild(border);
    var triangle = create_item(cid, "path", {
        d: numbox_data_string_triangle(w, h),
        fill: color,
        stroke: "black",
        "stroke-width": (drawstyle == 0 || drawstyle ==  2 ? 1 : 0),
        id: (tag + "triangle"),
        "class": (is_toplevel === 1 ? "toplevel " : "") + "border"
    });
    g.appendChild(triangle);
}

function gui_numbox_coords(cid, tag, w, h) {
    gui(cid).get_elem(tag + "border", {
        d: numbox_data_string_frame(w, h)
    });
    gui(cid).get_elem(tag + "triangle", {
        d: numbox_data_string_triangle(w, h)
    });
}

function gui_numbox_draw_text(cid, tag, text, font_size, color, xpos,
    ypos, basex, basey, fontmargin, is_toplevel) {
    // kludge alert -- I'm not sure why I need to add half to the ypos
    // below. But it works for most font sizes.
    //post("font_size=" + font_size);
    gui(cid).get_gobj(tag)
    .append(function(frag, w) {
        var trans_y = 0;
        // fine-tuning the translate y rule:
        if (font_size === 18 && fontmargin === 2) {
            trans_y = 16.5;
        } else if (font_size === 17 && fontmargin === 2) {
            trans_y = 15.5;
        } else {
            trans_y = (font_size - fontmargin/2);
        }
        var svg_text = create_item(cid, "text", {
            transform: "translate(" +
                        (xpos - basex) + "," + trans_y + ")",
                        //((ypos - basey + (ypos - basey) * 0.5)|0) + ")",
            "font-size": font_size,
            fill: color,
            id: tag + "text",
            class: "cut copy paste" + (is_toplevel === 1 ? " toplevel" : "")
        }),
        text_node = w.document.createTextNode(text);
        svg_text.appendChild(text_node);
        frag.appendChild(svg_text);
        return frag;
    });
}

function gui_numbox_update(cid, tag, fcolor, bgcolor, num_font_size, font_name, font_size, font_weight, drawstyle) {
    gui(cid)
    .get_elem(tag + "border", {
        fill: bgcolor,
        "stroke-width": (drawstyle < 2 ? 1 : 0),
    })
    .get_elem(tag + "triangle", {
        fill: bgcolor,
        "stroke-width": (drawstyle == 0 || drawstyle ==  2 ? 1 : 0),
    })
    .get_elem(tag + "text", {
        fill: fcolor,
        "font-size": num_font_size
    })
    // label may or may not exist, but that's covered by the API
    .get_elem(tag + "label", function() {
        gui_iemgui_label_font(cid, tag, font_name, font_weight, font_size);
    });
}

function gui_numbox_update_text_position(cid, tag, x, y, font_size, fontmargin) {
    var trans_y = 0;
    // fine-tuning the translate y rule:
    if (font_size === 18 && fontmargin === 2) {
        trans_y = 16.5;
    } else if (font_size === 17 && fontmargin === 2) {
        trans_y = 15.5;
    } else {
        trans_y = (font_size - fontmargin/2);
    }
    gui(cid).get_elem(tag + "text", {
        //transform: "translate(" + x + "," + ((y + y*0.5)|0) + ")"
        transform: "translate(" + x + "," + trans_y + ")"
    });
}

function gui_slider_new(cid, tag, color, p1, p2, p3, p4, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var indicator = create_item(cid, "line", {
            x1: p1 - basex,
            y1: p2 - basey,
            x2: p3 - basex,
            y2: p4 - basey,
            stroke: color,
            "stroke-width": 3,
            fill: "none",
            id: tag + "indicator"
        });
        frag.appendChild(indicator);
        return frag;
    });
}

function gui_slider_update(cid, tag, p1, p2, p3, p4, basex, basey) {
    gui(cid).get_elem(tag + "indicator", {
        x1: p1 - basex,
        y1: p2 - basey,
        x2: p3 - basex,
        y2: p4 - basey
    });
}

function gui_slider_indicator_color(cid, tag, color) {
    gui(cid).get_elem(tag + "indicator", {
        stroke: color
    });
}

function gui_radio_new(cid, tag, p1, p2, p3, p4, i, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var cell = create_item(cid, "line", {
            x1: p1 - basex,
            y1: p2 - basey,
            x2: p3 - basex,
            y2: p4 - basey,
            // stroke is just black for now
            stroke: "black",
            "stroke-width": 1,
            fill: "none",
            id: tag + "cell_" + i
        });
        frag.appendChild(cell);
        return frag;
    });
}

function gui_radio_create_buttons(cid ,tag, color, p1, p2, p3, p4, basex, basey, i, state) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var b = create_item(cid, "rect", {
            x: p1 - basex,
            y: p2 - basey,
            width: p3 - p1,
            height: p4 - p2,
            stroke: color,
            fill: color,
            id: tag + "button_" + i,
            display: state ? "inline" : "none"
        });
        frag.appendChild(b);
        return frag;
    });
}

function gui_radio_button_coords(cid, tag, x1, y1, xi, yi, i, s, d, orient) {
    gui(cid)
    .get_elem(tag + "button_" + i, {
        x: orient ? s : xi+s,
        y: orient ? yi+s : s,
        width: d-(s*2),
        height: d-(s*2)
    })
    // the line to draw the cell for i=0 doesn't exist. Probably was not worth
    // the effort, but it's easier just to check for that here atm.
    if (i > 0) {
        gui(cid)
        .get_elem(tag + "cell_" + i, {
            x1: orient ? 0 : xi,
            y1: orient ? yi : 0,
            x2: orient ? d : xi,
            y2: orient ? yi : d
        });
    }
}

function gui_radio_update(cid, tag, color, prev, next) {
    gui(cid)
    .get_elem(tag + "button_" + prev, {
        display: "none"
    })
    .get_elem(tag + "button_" + next, {
        display: "inline",
        fill: color,
        stroke: color
    });
}

function gui_vumeter_draw_text(cid, tag, color, xpos, ypos, text, index, basex, basey, font_size, font_weight) {
    gui(cid).get_gobj(tag)
    .append(function(frag, w) {
        var svg_text = create_item(cid, "text", {
            x: xpos - basex,
            y: ypos - basey,
            "font-family": iemgui_fontfamily(fontname),
            "font-size": font_size,
            "font-weight": font_weight,
            id: tag + "text_" + index
        }),
        text_node = w.document.createTextNode(text);
        svg_text.appendChild(text_node);
        frag.appendChild(svg_text);
        return frag;
    });
}

// Oh, what a terrible interface this is!
// the c API for vumeter was just spewing all kinds of state changes
// at tcl/tk, depending on it to just ignore non-existent objects.
// On changes in the Properties dialog, it would
// a) remove all the labels
// b) configure a bunch of _non-existent_ labels
// c) recreate all the missing labels
// To get on to other work we just parrot the insanity here,
// and silently ignore calls to update non-existent text.
function gui_vumeter_update_text(cid, tag, text, font, selected, color, i) {
    gui(cid).get_elem(tag + "text_" + i, {
        fill: color
    });
}

function gui_vumeter_text_coords(cid, tag, i, xpos, ypos, basex, basey) {
    gui(cid).get_elem(tag + "text_" + i, {
        x: xpos - basex,
        y: ypos - basey
    });
}

function gui_vumeter_erase_text(cid, tag, i) {
    gui(cid).get_elem(tag + "text_" + i, function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_vumeter_create_steps(cid, tag, color, p1, p2, p3, p4, width, basex, basey, i) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var l = create_item(cid, "line", {
            x1: p1 - basex,
            y1: p2 - basey,
            x2: p3 - basex,
            y2: p4 - basey,
            stroke: color,
            "stroke-width": width,
            "id": tag + "led_" + i
        });
        frag.appendChild(l);
        return frag;
    });
}

function gui_vumeter_update_steps(cid, tag, i, width) {
    gui(cid).get_elem(tag + "led_" + i, {
        "stroke-width": width
    });
}

function gui_vumeter_update_step_coords(cid, tag, i, x1, y1, x2, y2, basex, basey) {
    gui(cid).get_elem(tag + "led_" + i, {
        x1: x1 - basex,
        y1: y1 - basey,
        x2: x2 - basex,
        y2: y2 - basey
    });
}

function gui_vumeter_draw_rect(cid, tag, color, p1, p2, p3, p4, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var rect = create_item(cid, "rect", {
            x: p1 - basex,
            y: p2 - basey,
            width: p3 - p1,
            height: p4 + 1 - p2,
            stroke: color,
            fill: color,
            id: tag + "rect"
        });
        frag.appendChild(rect);
        return frag;
    });
}

function gui_vumeter_update_rect(cid, tag, color) {
    gui(cid).get_elem(tag + "rect", {
        fill: color,
        stroke: color
    });
}

// Oh hack upon hack... why doesn't the iemgui base_config just take care
// of this?
function gui_vumeter_border_size(cid, tag, width, height) {
    gui(cid).get_gobj(tag)
    .q(".border", {
        width: width,
        height: height
    });
}

function gui_vumeter_update_peak_width(cid, tag, width) {
    gui(cid).get_elem(tag + "rect", {
        "stroke-width": width
    });
}

function gui_vumeter_draw_peak(cid, tag, color, p1, p2, p3, p4, width, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var line = create_item(cid, "line", {
            x1: p1 - basex,
            y1: p2 - basey,
            x2: p3 - basex,
            y2: p4 - basey,
            stroke: color,
            "stroke-width": width,
            id: tag + "peak"
        }); 
        frag.appendChild(line);
        return frag;
    });
}

// probably should change tag from "rect" to "cover"
function gui_vumeter_update_rms(cid, tag, p1, p2, p3, p4, basex, basey) {
    gui(cid).get_elem(tag + "rect", {
        x: p1 - basex,
        y: p2 - basey,
        width: p3 - p1,
        height: p4 - p2 + 1
    });
}

function gui_vumeter_update_peak(cid,tag,color,p1,p2,p3,p4,basex,basey) {
    gui(cid).get_elem(tag + "peak", {
        x1: p1 - basex,
        y1: p2 - basey,
        x2: p3 - basex,
        y2: p4 - basey,
        stroke: color
    });
}

function gui_iemgui_base_color(cid, tag, color) {
    gui(cid).get_gobj(tag)
    .q(".border", {
        fill: color
    });
}

function gui_iemgui_move_and_resize(cid, tag, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag, function(e) {
        elem_move(e, x1, y1);
    })
    .q(".border", {
        width: x2 - x1,
        height: y2 - y1
    });
}

function iemgui_font_height(name, size) {
    return size;
    var dejaVuSansMono = {
        6: [3, 4], 7: [4, 5], 8: [5, 7], 9: [5, 7], 10: [6, 8],
        11: [7, 8], 12: [7, 9], 13: [8, 9], 14: [8, 10], 15: [9, 12],
        16: [9, 12], 17: [10, 13], 18: [10, 13], 19: [11, 14], 20: [12, 14],
        21: [12, 16], 22: [13, 16], 23: [13, 17], 24: [14, 18], 25: [14, 18],
        26: [15, 20], 27: [16, 20], 28: [16, 21], 29: [17, 21], 30: [17, 22],
        31: [18, 22], 32: [18, 23], 33: [19, 25], 34: [19, 25], 35: [20, 26],
        36: [21, 26], 37: [21, 27], 38: [22, 27], 39: [22, 29], 40: [23, 30],
        41: [23, 30], 42: [24, 31], 43: [24, 31], 44: [25, 33], 45: [26, 33],
        46: [25, 34], 47: [26, 34], 48: [26, 35], 49: [27, 36], 50: [26, 36],
        51: [28, 37], 52: [29, 38], 53: [29, 39], 54: [30, 39], 55: [30, 41],
        56: [31, 41], 57: [31, 42], 58: [32, 43], 59: [32, 43], 60: [32, 45],
        61: [34, 45], 62: [34, 46], 63: [35, 46], 64: [35, 47], 65: [36, 49],
        66: [36, 49], 67: [36, 50], 68: [37, 50], 69: [38, 51], 70: [38, 51],
        71: [38, 52], 72: [39, 52]
    };
    // We use these heights for both the monotype and iemgui's "Helvetica"
    // which, at least on linux, has the same height
    if (name === "DejaVu Sans Mono" || name == "helvetica") {
        return dejaVuSansMono[size][1];
    } else {
        return size;
    }
}

function iemgui_fontfamily(name) {
    var family = "DejaVu Sans Mono";
    if (name === "DejaVu Sans Mono") {
        family = "DejaVu Sans Mono"; // probably should add some fallbacks here
    }
    else if (name === "helvetica") {
        family = "Helvetica, 'DejaVu Sans'";
    }
    else if (name === "times") {
        family = "'Times New Roman', 'DejaVu Serif', 'FreeSerif', serif";
    }
    return family;
}

function gui_iemgui_label_new(cid, tag, x, y, color, text, fontname, fontweight,
    fontsize) {
    //post("gui_iemgui_label_new " + color);
    gui(cid).get_gobj(tag)
    .append(function(frag, w) {
        var svg_text = create_item(cid, "text", {
            // x and y need to be relative to baseline instead of nw anchor
            x: x,
            y: y,
            //"font-size": font + "px",
            "font-family": iemgui_fontfamily(fontname),
            // for some reason the font looks bold in Pd-Vanilla-- not sure why
            "font-weight": fontweight,
            "font-size": fontsize + "px",
            fill: color,
            // Iemgui labels are anchored "w" (left-aligned to non-tclers).
            // For no good reason, they are also centered vertically, unlike
            // object box text. Since svg text uses the baseline as a reference
            // by default, we just take half the pixel font size and use that
            // as an additional offset.
            //
            // There is an alignment-baseline property in svg that
            // is supposed to do this for us. However, when I tried choosing
            // "hanging" to get tcl's equivalent of "n", I ran into a bug
            // where the text gets positioned incorrectly when zooming.
            transform: "translate(0," +
                iemgui_font_height(fontname, fontsize) / 2 + ")",
            id: tag + "label"
        });
        // make sure that spaces properly show
        svg_text.style.setProperty("white-space", "pre");
        var text_node = w.document.createTextNode(text);
        svg_text.appendChild(text_node);
        frag.appendChild(svg_text);
        return frag;
    });
}

function gui_iemgui_label_set(cid, tag, text) {
    gui(cid).get_elem(tag + "label", function(e) {
        e.textContent = text; 
    });
}

function gui_iemgui_label_coords(cid, tag, x, y) {
    gui(cid).get_elem(tag + "label", {
        x: x,
        y: y
    });
}

function gui_iemgui_label_color(cid, tag, color) {
    gui(cid).get_elem(tag + "label", {
        fill: color
    });
}

function gui_iemgui_label_select(cid, tag, is_selected) {
    gui(cid).get_elem(tag + "label", function(e) {
        if (!!is_selected) {
            e.classList.add("iemgui_label_selected");
        } else {
            e.classList.remove("iemgui_label_selected");
        }
    });
}

function gui_iemgui_label_font(cid, tag, fontname, fontweight, fontsize) {
    gui(cid).get_elem(tag + "label", {
        "font-family": iemgui_fontfamily(fontname),
        "font-weight": fontweight,
        "font-size": fontsize + "px",
        transform: "translate(0," + iemgui_font_height(fontname, fontsize) / 2 + ")"
    });
}

function toggle_drag_handle_cursors(e, is_label, state) {
    e.querySelector(".constrain_top_right").style.cursor =
        state ? "ew-resize" : "";
    e.querySelector(".constrain_bottom_right").style.cursor =
        state ? "ns-resize" : "";
    e.querySelector(".unconstrained").style.cursor =
        state ? (is_label ? "move" : "se-resize") : "";
}

exports.toggle_drag_handle_cursors = toggle_drag_handle_cursors;

// Show or hide little handle for dragging around
// iemgui labels, cnv resize hooks, or gop resize/move hooks
function gui_iemgui_label_show_drag_handle(cid, tag, state, x, y, cnv_resize, is_toplevel) {
    //post("gui_iemgui_label_show_drag_handle tag=" + tag + " state=" + state);
    if (state !== 0) {
        gui(cid).get_gobj(tag)
        .append(function(frag, w) {
            var g, rect, top_right, bottom_right;
            g = create_item(cid, "g", {
                class: (cid === tag) ? "gop_drag_handle move_handle " +
                    (is_toplevel === 1 ? " toplevel" : "") + " border" :
                    cnv_resize !== 0 ? "cnv_resize_handle" +
                    (is_toplevel === 1 ? " toplevel" : "") + " border" :
                    "label_drag_handle move_handle" + (is_toplevel === 1 ? " toplevel" : "")
                    + " border",
                transform: "matrix(1, 0, 0, 1, 0, 0)"
            });
            // Here we use a "line" shape so that we can control its color
            // using the "border" class (for iemguis) or the "gop_rect" class
            // for the graph-on-parent rectangle anchor. In both cases the
            // styles set a stroke property, and a single thick line is easier
            // to define than a "rect" for that case.
            if (cid === tag) {
                rect = create_item(cid, "path", {
                    d: "M 1 1 L 15 1 L 1 15 z",
                    class: "unconstrained",
                    fill: "red",
                    stroke: "red",
                    opacity: 0.5
                });
            } else if (cnv_resize) {
                // if we are mycanvas resize hook found in the bottom right corner
                rect = create_item(cid, "path", {
                    d: "M " + (x-18) + " " + (y-2) + " L " + (x-10) + 
                        " " + (y-2) + " L " + (x-10) + " " + (y-10) + " z",
                    class: "unconstrained",
                    fill: "red",
                    stroke: "red",
                    opacity: 0
                });
            } else {
                // otherwise we are a label hook
                /*rect = create_item(cid, "path", {
                    d: "M " + (x+8) + " " + y + " L " + (x+20) +
                        " " + y + " L " + (x+8) + " " + (y+12) + " z",
                    class: "unconstrained",
                    fill: "red",
                    opacity: 0.5
                */
                rect = create_item(cid, "line", {
                    x1: x+5,
                    y1: y+3,
                    x2: x+5,
                    y2: y + 9,
                    "stroke-width": 6,
                    class: "unconstrained",
                    //fill: "red",
                    //stroke: "red",
                    //opacity: 0.5
                });
            }
            g.classList.add("clickable_resize_handle");
            if (cnv_resize) {
                top_right = create_item(cid, "rect", {
                    x:      (x-11 >= 0 ? x-11 : 0),
                    y:         0,
                    width:     3,
                    height:  (y-8 >= 0 ? y-8 : 0),
                    fill:   "rgba(0,255,0,0)",
                    stroke: "rgba(0,255,0,0)",
                    class: "constrain_top_right"
                });
                bottom_right = create_item(cid, "rect", {
                    x:        0,
                    y:     (y-3 >= 0 ? y-3 : 0),
                    width: (x-16 >= 0 ? x-16 : 0),
                    height:   3,
                    fill:   "rgba(0,0,255,0)",
                    stroke: "rgba(0,0,255,0)",
                    class: "constrain_bottom_right"
                });                
            } else {
                /* experimental new approach, I don't like the cursor
                   blocking the view of the label, so the user is
                   unable to see how the label fits with the rest
                top_right = create_item(cid, "rect", {
                    x: x +  (cid === tag ? 4 : 8),
                    y: y +  (cid === tag ? 15.5 : 3),
                    width:  2,
                    height: (cid === tag ? 11 : 9),
                    fill:   "rgba(0,255,0,0)",
                    stroke: "rgba(0,255,0,0)",
                    class: "constrain_top_right"
                });
                bottom_right = create_item(cid, "rect", {
                    x: x + 11,
                    y: y +  (cid === tag ? 11 : 0),
                    width:  (cid === tag ? 11 : 9),
                    height: 2,
                    fill:   "rgba(0,0,255,0)",
                    stroke: "rgba(0,0,255,0)",
                    class: "constrain_bottom_right"
                });
                */
                top_right = create_item(cid, "rect", {
                    x: x +  (cid === tag ? 4 : 2.5),
                    y: y +  (cid === tag ? 15.5 : 3.5),
                    width:  (cid === tag ? 3 : 1),
                    height: (cid === tag ? 11 : 5),
                    fill:   "rgba(0,255,0,0)",
                    stroke: "rgba(0,255,0,0)",
                    "fill-opacity": "0",
                    class: "constrain_top_right"
                });
                bottom_right = create_item(cid, "rect", {
                    x: x + 4.5,
                    y: y +  (cid === tag ? 11 : 3.5),
                    width:  (cid === tag ? 11 : 3),
                    height: (cid === tag ? 3 : 1),
                    fill:   "rgba(0,0,255,0)",
                    stroke: "rgba(0,0,255,0)",
                    "fill-opacity": "0",
                    class: "constrain_bottom_right"
                });
            }
            g.appendChild(rect);
            g.appendChild(top_right);
            g.appendChild(bottom_right);

            // Quick hack for cursors on mouse-over. We only add them if
            // we're not already dragging a label or resizing an iemgui.
            // Apparently I didn't register all these edge-case event states
            // in canvas_events. States like "iemgui_label_drag" actually
            // just get registered as state "none". So we just check for "none"
            // here and assume it means we're in the middle of dragging.
            // If not we go ahead and set our cursor styles.
            if (w.canvas_events.get_state() != "none") {
                toggle_drag_handle_cursors(g, cnv_resize === 0, true);
            }

            frag.appendChild(g);
            return frag;
        });
    } else {
        gui(cid).get_gobj(tag, function(e) {
            var g =
                e.getElementsByClassName((cid === tag) ? "gop_drag_handle" :
                    cnv_resize !== 0 ? "cnv_resize_handle" :
                        "label_drag_handle")[0];
            //rect = get_item(cid, "clickable_resize_handle");
            // Need to check for null here...
            if (g) {
                g.parentNode.removeChild(g);
            }/* else {
                post("error: couldn't delete the iemgui drag handle!");
            }*/
        });
    }
}

function gui_iemgui_label_displace_drag_handle(cid, tag, dx, dy) {
    gui(cid).get_gobj(tag)
    .q(".label_drag_handle", function(e) {
        var t = e.transform.baseVal.getItem(0);
        t.matrix.e += dx;
        t.matrix.f += dy;
    });
}

function gui_mycanvas_new(cid, ownercid, parentcid, tag, color, x1, y1,
    x2_vis, y2_vis, x2, y2, is_toplevel) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var rect_vis, rect, g;
        rect_vis = create_item(cid, "rect", {
            width: x2_vis - x1,
            height: y2_vis - y1,
            fill: color,
            stroke: color,
            id: tag + "rect"
            }
        );
        // we use a drag_handle, which is square outline with transparent fill
        // that shows the part of the rectangle that may be dragged in editmode.
        // Clicking the rectangle outside of that square will have no effect.
        // Unlike a 'border' it takes the same color as the visible rectangle
        // when deselected.
        // I'm not sure why it was decided to define this object's bbox separate
        // from the visual rectangle. That causes all kinds of usability
        // problems.
        // For just one example, it means we can't simply use the "resize"
        // cursor like all the other iemguis.
        // Unfortunately its ingrained as a core object in Pd, so we have to
        // support it here.
        rect = create_item(cid, "rect", {
            width: x2 - x1,
            height: y2 - y1,
            fill: "none",
            stroke: color,
            id: tag + "drag_handle",
            "class": (is_toplevel === 1 ? "toplevel " : "") + "border mycanvas_border"
            }
        );
        frag.appendChild(rect_vis);
        frag.appendChild(rect);
        return frag;
    });
}

function gui_mycanvas_update(cid, tag, color, selected) {
    gui(cid)
    .get_elem(tag + "rect", {
        fill: color,
        stroke: color
    })
    .get_elem(tag + "drag_handle", {
        stroke: color
    });
}

function gui_mycanvas_coords(cid, tag, vis_width, vis_height, select_width, select_height) {
    gui(cid)
    .get_elem(tag + "rect", {
        width: vis_width,
        height: vis_height
    })
    .get_elem(tag + "drag_handle", {
        width: select_width,
        height: select_height
    });
}

/* this creates a group immediately below the patchsvg object.
   this does not have the dimensions, just the x and y info.
*/
function gui_scalar_new(cid, ownercid, parentcid, tag, isselected, t1, t2, t3, t4, t5, t6,
    is_toplevel, plot_style) {
    /*
    post("gui_scalar_new drawon=" + cid + " ownercid=" + ownercid +
    " parentcid=" + parentcid + " tag=" + tag + " isselected=" + isselected +
    " plot_style=" + plot_style + " t1=" + t1 + " t2=" + t2 + " t3=" + t3 + " t4=" + t4 +
    " xpos(t5)=" + t5 + " ypos(t6)=" + t6 + " is_toplevel=" + is_toplevel);
    */
    var g, draw_xpos, draw_ypos;
    draw_xpos = t5;
    draw_ypos = t6;
    //post("...SCALAR classname=" + ownercid + "svg");
    gui(cid).get_elem("patchsvg", function(svg_elem, w) {
        var transform_string;
        if (is_toplevel === 0) {
            var tgt = w.document.getElementsByClassName(ownercid + "svg");
            var svg_view_box = svg_elem.getAttribute("viewBox").split(" ");
            draw_xpos = draw_xpos - tgt[0].getScreenCTM().e +
                svg_elem.getBoundingClientRect().left - svg_view_box[0];
            draw_ypos = draw_ypos - tgt[0].getScreenCTM().f +
                svg_elem.getBoundingClientRect().top - svg_view_box[1];
            /*
            post("......parentcid != drawcid\n" +
                "   t5=" + t5 + " t6=" + t6 + "\n" +
                //"   scroll=(" + scrollX + "," + scrollY +")\n" +
                "   svg_elem screenCTM=(" + svg_elem.getScreenCTM().e + "," +
                    svg_elem.getScreenCTM().f + ")\n" +
                "   svg elem CTM=(" + svg_elem.getCTM().e + "," +
                    svg_elem.getCTM().f + ")\n" +
                "   tgt[0].getScreenCTM=(" + tgt[0].getScreenCTM().e + "," + 
                    tgt[0].getScreenCTM().f + ")\n" +
                "   view_box=(" + svg_view_box[0] + "," + svg_view_box[1] + ")\n" +
                "   scroll=(" + svg_elem.scrollLeft + "," + svg_elem.scrollTop + ")\n" +
                "   rect=(" +  svg_elem.getBoundingClientRect().left + "," +
                    svg_elem.getBoundingClientRect().top + ")\n" +
                "   drawxy=(" + draw_xpos + "," + draw_ypos + ")"
            );
            */
        }

        // these offsets are used only for the drawing inside a GOP
        // they should be ignored for the top-level scalars
        if (plot_style >= 0) {
            draw_xpos += 0.5;
            draw_ypos -= 1;
        }
        //post("plot_style=" + plot_style);

        /* ico@vt.edu HACKTASCTIC: calculating scrollbars is throwing 0.997 for
           plots drawn inside the subpatch and it is a result of the -1 in the
           (min_width - 1) / width call inside canvas_params. Yet, if we don't
           call this, we don't have nice flush scrollbars with the regular edges.
           This is why here we make a hacklicious hack and simply hide hscrollbar
           since the scroll is not doing anything anyhow.

           After further testing, it seems that the aforesaid margin is a hit'n'miss
           depending on the patch, so we will disable this and make the aforesaid
           canvas_params equation min_width / width.

        if (is_toplevel === 1) {
            gui(cid).get_elem("hscroll", function(elem) {
                elem.style.setProperty("display", "none");
            });
            gui(cid).get_elem("vscroll", function(elem) {
                elem.style.setProperty("display", "none");
            });
        }*/

        gui(cid).get_elem("patchsvg", function(svg_elem) {
            var matrix, transform_string, selection_rect;
            if (is_toplevel === 1) {
                // here we deal with weird scrollbar offsets and
                // inconsistencies for the various plot styles.
                // the matrix format is xscale, 0, 0, yscale, width, height
                // we don't use the matrix for the bar graph since it is
                // difficult to get the right ratio, so we do the manual
                // translate and scale instead.
                // cases are: 0=points, 1=plot, 2=bezier, 3=bars
                switch (plot_style) {
                    case 0:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos];
                        break;
                    case 1:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos];
                        break;
                    case 2:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos];
                        break;
                    case 3:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos];
                        break;
                    /*case 3:
                        //matrix = [t1*.995,t2,t3,t4+1,t5+0.5,t6-2];
                        matrix = 0;
                        transform_string = "translate(" + 0 +
                            "," + (draw_ypos+1) + ") scale(" + t1 + "," + t4 + ")";
                        //post("transform_string = " + transform_string);
                        break;*/
                    default:
                        // we are a top-level non-plot scalar
                        // this includes things like subpatches with scalars only
                        // that need to be resized based on the window size and/or
                        // scalar arrays (see all_about_arrays.pd help patch),which
                        // should not be scaled. how to distinguish between the two?
                        //post("......non-plot toplevel scalar");
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos];
                        break;
                }
            }
            else {
                // ico@vt.edu 2022-09-29: here we squash the x offset if we are
                // drawing an array inside a GOP since it always starts flush with
                // the left side
                //if (plot_style > -1)
                //    draw_xpos = 0;
                switch (plot_style) {
                    case 0:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos+0.5];
                        break;
                    case 1:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos+1.5];
                        break;
                    case 2:
                        matrix = [t1,t2,t3,t4,draw_xpos,draw_ypos+1.5];
                        break;
                    case 3:
                        //matrix = [t1,t2,t3,t4+1,t5+0.5,t6+0.5];
                        matrix = 0;
                        transform_string = "translate(" + (draw_xpos+(t1 < 1 ? 0.5 : 1.5)) +
                            "," + (draw_ypos+1.5) + ") scale(" + t1 + "," + t4 + ")";
                        //post("transform_string = " + transform_string);
                        break;
                    default:
                        // we are a non-plot scalar
                        //post("......non-plot GOP scalar");
                        matrix = [t1,t2,t3,t4,draw_xpos+0.5,draw_ypos+0.5];
                        break;
                }
            }

            if (matrix !== 0) {
                transform_string = "matrix(" + matrix.join() + ")";
            }
            g = create_item(cid, "g", {
                id: tag + "gobj",
                transform: transform_string,
                class: "scalar"
            });
            if (isselected !== 0) {
                g.classList.add("selected");
            }
            if (is_toplevel === 0) {
                g.classList.add("gop");
            }
            /*
            //currently unused because we use toplevel_scalars instead
            else if (plot_style === -1) //else implies is_toplevel is true
            {
                // this means this scalar is not a part of an array
                // and if it is in a toplevel window, every time
                // window is resized, we need to redraw it to ensure
                // it is scaled accordingly
                redraw_on_resize = 1;
            }
            */
            // Let's make a selection rect...
            selection_rect = create_item(cid, "rect", {
                class: "border" + (plot_style == -2 ? " empty" : ""),
                "pointer-events": "none",
                // ico@vt.edu 2022-10-17: set default size in case
                // we don't have a template.
                width: 5,
                height: 5,
                x: 0.5,
                y: 0.5,
                "plot_style" : plot_style
            });
            g.appendChild(selection_rect);
            //add_gobj_to_svg(svg_elem, g);
            if (is_toplevel === 0) {
                //post("...GOP appendChild cid=" + cid + " tgt=" + ownercid + "svg");
                tgt[0].appendChild(g);
            } else {
                //post("...add_gop_to_svg");
                add_gobj_to_svg(svg_elem, g);
            }
        });
    });
    return g;
}

function gui_scalar_erase(cid, tag) {
    //post("gui_scalar_erase owner=" + cid + " object=" + tag);
    gui(cid).get_gobj(tag, function(e) {
        e.parentNode.removeChild(e);
    });
}

// This is unnecessarily complex--the select rect is a child of the parent
// scalar group, but in the initial Tkpath API the rect was free-standing.
// This means all the coordinate parameters are in the screen position. But
// we need the coords relative to the scalar's x/y--hence we subtract the
// scalar's basex/basey from the coords below.

// Additionally, this function is a misnomer--we're not actually drawing
// the rect here. It's drawn as part of the scalar_vis function. We're
// merely changing its coords and size.

// Finally, we have this awful display attribute toggling in css
// for selected borders because somehow calling properties on a graph
// triggers this function.  I have no idea why it does that.
function gui_scalar_draw_select_rect(cid, tag, state, x1, y1, x2, y2, basex, basey) {
    /*
    post("gui_scalar_draw_select_rect tag=" + tag + " x1=" + x1 +
         " x2=" + x2 + " y1=" +y1 + " y2=" + y2 +
         " basex=" + basex + " basey=" + basey);
    */
    var plot_style;
    gui(cid).get_gobj(tag)
    .q(".border", function(b) {
        plot_style = b.getAttribute("plot_style");
    });

    gui(cid).get_gobj(tag)
    .q(".border", {
        x: (x1 - basex) + 0.5,
        y: (y1 - basey) + 0.5,
        width: (plot_style < 0 && state == 0 ? 5 : x2 - x1),
        height: (plot_style < 0 && state == 0 ? 5 : y2 - y1)
    });
}

function gui_scalar_draw_group(cid, tag, parent_tag, type, attr_array) {
    //post("gui_scalar_draw_group type=" + type + " attr_array=" + attr_array);
    gui(cid).get_elem(parent_tag)
    .append(function(frag) {
        if (!attr_array) {
            attr_array = [];
        }
        attr_array.push("id", tag);
        var group = create_item(cid, type, attr_array);
        frag.appendChild(group);
        return frag;
    });
}

function gui_scalar_configure_gobj(cid, tag, is_toplevel, isselected, t1, t2, t3, t4, t5, t6) {
    /*
    post("gui_scalar_configure_gobj tag=" + tag + " t1=" +
        t1 + " t2=" + t2 + " t3=" + t3 + " t4=" + t4 + " t5=" + t5 + " t6=" + t6);
    */
    // adding offset when drawing inside GOP
    if (is_toplevel === 0) {
        t5 += 1;
    }
    var matrix = [t1,t2,t3,t4,t5,t6],
        transform_string = "matrix(" + matrix.join() + ")";
    gui(cid).get_gobj(tag, {
        transform: transform_string 
    });
}

function gui_draw_vis(cid, type, attr_array, tag_array) {
    //post("gui_draw_vis attr_array=" + attr_array + " tag_array=" + tag_array);
    gui(cid).get_elem(tag_array[0])
    .append(function(frag) {
        var item;
        attr_array.push("id", tag_array[1]);
        item = create_item(cid, type, attr_array);
        frag.appendChild(item);
        return frag;
    });
}

// This is a stop gap to update the old draw commands like [drawpolygon]
// without having to erase and recreate their DOM elements
function gui_draw_configure_old_command(cid, type, attr_array, tag_array) {
    gui(cid).get_elem(tag_array[1], function(e) {
        configure_item(e, attr_array);
    });
}

function gui_draw_erase_item(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_draw_coords(cid, tag, shape, points) {
    gui(cid).get_elem(tag, function(elem) {
        switch (shape) {
            case "rect":
                configure_item(elem, {
                    x: points[0],
                    y: points[1],
                    width: points[2],
                    height: points[3]
                });
                break;
            case "circle":
                configure_item(elem, {
                    cx: points[0],
                    cy: points[1]
                });
                break;
            case "polyline":
            case "polygon":
                configure_item(elem, {
                    points: points
                });
                break;
            default:
        }
    });
}

// set a drag event for a shape that's part of a scalar.
// this is a convenience method for the user, so that dragging outside
// of the bbox of the shape will still register as part of the event.
// (Attempting to set the event more than once is ignored.)
function gui_draw_drag_event(cid, tag, scalar_sym, drawcommand_sym,
    event_name, array_sym, index, state) {
    gui(cid).get_elem("patchsvg", function(svg_elem, w) {
        if (state === 0) {
            w.canvas_events.remove_scalar_draggable(tag);
        } else {
            w.canvas_events.add_scalar_draggable(cid, tag, scalar_sym,
                drawcommand_sym, event_name, array_sym, index);
        }
    });
}

// Events for scalars-- mouseover, mouseout, etc.
function gui_draw_event(cid, tag, scalar_sym, drawcommand_sym, event_name,
    array_sym, index, state) {
    gui(cid).get_elem(tag, function(e) {
        var event_type = "on" + event_name;
        if (state === 1) {
            e[event_type] = function(e) {
                pdsend(cid, "scalar_event", scalar_sym, drawcommand_sym,
                    array_sym, index, event_name, e.pageX, e.pageY);
            };
        } else {
            e[event_type] = null;
        }
    });
}

// Configure one attr/val pair at a time, received from Pd
function gui_draw_configure(cid, tag, attr, val) {
    gui(cid).get_elem(tag, function(e) {
        var obj = {};
        if (Array.isArray(val)) {
            obj[attr] = val.join(" ");
        } else {
            // strings or numbers
            obj[attr] = val;
        }
        configure_item(e, obj);
    });
}

// Special case for viewBox which, in addition to its inexplicably inconsistent
// camelcasing also has no "none" value in the spec. This requires us to create
// a special case to remove the attribute if the user wants to get back to
// the default behavior.
function gui_draw_viewbox(cid, tag, attr, val) {
    // Value will be an empty array if the user provided no values
    //post("gui_draw_viewbox cid=" + cid + " tag=" + tag + " attr=" + attr + " val=" + val);
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        if (val.length) {
            gui_draw_configure(cid, tag, attr, val)
        } else {
            get_item(cid, tag).removeAttribute("viewBox");
        }
    });
}

// Configure multiple attr/val pairs (this should be merged with gui_draw_configure at some point
function gui_draw_configure_all(cid, tag, attr_array) {
    gui(cid).get_elem(tag, attr_array);
}

// Plots for arrays and data structures
function gui_plot_vis(cid, basex, basey, data_array, attr_array, tag_array) {
    gui(cid).get_elem(tag_array[0])
    .append(function(frag) {
        var p = create_item(cid, "path", {
            d: data_array.join(" "),
            id: tag_array[1],
            //stroke: "red",
            //fill: "black",
            //"stroke-width": "0"
        });
        configure_item(p, attr_array);
        frag.appendChild(p);
        return frag;
    });
}

// This function doubles as a visfn for drawnumber. Furthermore it doubles
// as a way to update attributes for drawnumber/symbol without having to
// recreate the object. The "flag" argument is 1 for creating a new element,
// and -1 to set attributes on the existing object.
function gui_drawnumber_vis(cid, parent_tag, tag, x, y, scale_x, scale_y,
    font, fontsize, fontcolor, text, flag, visibility) {
    if (flag === 1) {
        gui(cid).get_elem(parent_tag)
        .append(function(frag) {
            var svg_text = create_item(cid, "text", {
                // x and y are fudge factors. Text on the tk canvas used an
                // anchor at the top-right corner of the text's bbox.  SVG uses
                // the baseline. There's probably a programmatic way to do this,
                // but for now-- fudge factors based on the DejaVu Sans Mono
                // font. :)

                // For an explanation of why we translate by "x" instead of
                // setting the x attribute, see comment in gui_text_new
                transform: "scale(" + scale_x + "," + scale_y + ") " +
                           "translate(" + x + ")",
                y: y + fontsize,
                // Turns out we can't do 'hanging' baseline because it's borked
                // when scaled. Bummer...
                // "dominant-baseline": "hanging",
                //"shape-rendering": "optimizeSpeed",
                "font-size": fontsize + "px",
                fill: fontcolor,
                visibility: visibility === 1 ? "normal" : "hidden",
                id: tag
            });
            // fill svg_text with tspan content by splitting on "\n"
            text_to_tspans(cid, svg_text, text);
            frag.appendChild(svg_text);
            return frag;
        });
    } else {
        gui(cid).get_elem(tag, function(svg_text) {
            configure_item(svg_text, {
                transform: "scale(" + scale_x + "," + scale_y + ") " +
                           "translate(" + x + ")",
                y: y + fontsize,
                // Turns out we can't do 'hanging' baseline because it's borked
                // when scaled. Bummer...
                // "dominant-baseline": "hanging",
                //"shape-rendering": "optimizeSpeed",
                "font-size": fontsize + "px",
                fill: fontcolor,
                visibility: visibility === 1 ? "normal" : "hidden",
                id: tag
            });
            svg_text.textContent = "";
            text_to_tspans(cid, svg_text, text);
        });
    }
}

// closure to handle class-specific data that
// needs to be in the GUI. There shouldn't be
// many cases for this-- for now it's just used
// to cache image data for image-handling classes:
// ggee/image
// moonlib/image (for backwards compatibility only: its API is inherently leaky)
// tof/imagebang
// draw sprite
// draw image

// ico@vt.edu 2021-10-19: added types, as follows:
// 0 = uninitialized
// 1 = ggee/image
// 2 = moonlib/image
var pd_cache = (function() {
    var d = {};
    var bbox = {};
    return {
        free: function(key) {
            if (d.hasOwnProperty(key)) {
                d[key] = null;
            }
        },
        set: function(key, data) {
            d[key] = data;
            return data;
        },
        set_svg_bbox: function(key, x, y, width, height) {
            bbox[key] = [ x, y, width, height ];
        },
        get: function(key) {
            if (d.hasOwnProperty(key)) {
                return d[key];
            } else {
                return undefined;
            }
        },
        get_svg_bbox: function(key) {
            if (d.hasOwnProperty(key)) {
                return bbox[key];
            } else {
                return undefined;
            }
        },
        getAllData: function() {
            return d;
        }
    };
}());

exports.pd_cache = pd_cache;

function gui_drawimage_new(obj_tag, file_path, canvasdir, flags) {
    //post("gui_drawimage_new");
    var drawsprite = 1,
        drawimage_data = [], // array for base64 image data
        image_seq,
        count = 0,
        matchchar = "*",
        files,
        ext,
        img_types = [".gif", ".jpeg", ".jpg", ".png", ".svg"],
        img; // dummy image to measure width and height
    image_seq = flags & drawsprite;
    if (file_path !== "") {
        if(!path.isAbsolute(file_path)) {
            file_path = path.join(canvasdir, file_path);
        }
        file_path = path.normalize(file_path);
    }
    if (file_path !== "" && fs.existsSync(file_path)) {
        if (image_seq && fs.lstatSync(file_path).isDirectory()) {
            // [draw sprite]
            files = fs.readdirSync(file_path)
                    .sort(); // Note that js's "sort" method doesn't do the
                             // "right thing" for numbers. For that we'd need
                             // to provide our own sorting function
        } else {
            // [draw image]
            files = [path.basename(file_path)];
            file_path = path.dirname(file_path);
        }
        // todo: warn about image sequence with > 999
        files.forEach(function(file) {
            ext = path.extname(file).toLowerCase();
            if (img_types.indexOf(ext) != -1) {
                // Now add an element to that array with the image data
                drawimage_data.push({
                    type: ext === ".jpeg" ? "jpg" : ext.slice(1),
                    data: fs.readFileSync(path.join(file_path, file),"base64")
                });
                count++;
            }
        });
    }

    if (count === 0) {
        // set a default image
        drawimage_data.push({
            type: "png",
            data: get_default_png_data()
        });
        if (file_path !== "") {
            post("draw image: error: couldn't load image");
        }
        post("draw image: warning: no image loaded. Using default png");
    }
    img = new pd_window.Image(); // create an image in the pd_window context
    img.onload = function() {
        pdsend(obj_tag, "size", this.width, this.height);
    };
    img.src = "data:image/" + drawimage_data[0].type +
        ";base64," + drawimage_data[0].data;
    pd_cache.set(obj_tag, drawimage_data); // add the data to container
}

function gui_image_free(obj_tag) {
    var c = pd_cache.get(obj_tag);
    if (c) {
        pd_cache.free(obj_tag); // empty the image(s)
    /*} else {
        post("image: warning: no image data in cache to free"); */
    }
}

function gui_image_free_loaded_images(objType) {
    var key;
    //var tally = 0;
    var keys = pd_cache.getAllData();
    //post("keys="+keys);
    for (key in keys) {
        //post("...key:"+key);
        if (keys.hasOwnProperty(key))
            //post("has property "+ keys[key].creator);
            if (keys[key] && keys[key].creator == objType) {
                gui_image_free(key);
                //tally++;
            }
    }
    //post("pd_cache length for type " + objType + " " + tally);
}

// We use this to get the correct height and width for the svg
// image. Unfortunately svg images are less flexible than normal
// html images-- you have to provide a size and 100% doesn't work.
// So here we load the image data into a new Image, just to get it
// to calculate the dimensions. We then use those dimensions for
// our svg image x/y, after which point the Image below _should_ 
// get garbage collected.
// We add the "tk_anchor" parameter so that we can match the awful interface
// of moonlib/image. We only check for the value "center"-- otherwise we
// assume "nw" (top-left corner) when tk_anchor is undefined, as this matches
// the svg spec.
function img_size_setter(cid, svg_image_tag, type, data, tk_anchor) {
    //post("img_size_setter " + type + " " + svg_image_tag);
    var img = new pd_window.window.Image(),
        w, h;
    img.onload = function() {
        w = this.width,
        h = this.height;
        //post("img_size_setter onload " + svg_image_tag);
        configure_item(get_item(cid, svg_image_tag), {
            width: w,
            height: h,
            x: tk_anchor === "center" ? 0 - w/2 : 0,
            y: tk_anchor === "center" ? 0 - h/2 : 0
        }); 
        gui(cid).get_elem(svg_image_tag, function(e) {
            if (e.getAttribute("type") !== "0") {
                //post("img_size_setter -> moonlib -> block");
                e.style.setProperty("display", "block");
            }
        });
    };
    img.src = "data:image/" + type + ";base64," + data;
}

function img_size_resizable_setter(cid, svg_image_tag, type, data, tk_anchor, w, h, constrain) {
    //post("img_size_resizable_setter " + type);
    var img = new pd_window.window.Image(),
        imgw, imgh;
    img.onload = function() {
        imgw = this.width,
        imgh = this.height;
        //post("img_size_resizable_setter onload " + svg_image_tag);
        configure_item(get_item(cid, svg_image_tag), {
            width: w,
            height: h,
            orig_width: imgw,
            orig_height: imgh,
            x: tk_anchor === "center" ? 0 - w/2 : 0,
            y: tk_anchor === "center" ? 0 - h/2 : 0,
        });
        gui(cid).get_elem(svg_image_tag, function(e) {
            if (e.getAttribute("type") !== "0") {
                //post("img_size_resizable_setter -> moonlib -> block");
                e.style.setProperty("display", "block");
            }
        });
    };
    img.src = "data:image/" + type + ";base64," + data;
}

function gui_ggee_image_resize(cid, svg_image_tag, w, h, resizemode, constrain) {
    //post("gui_ggee_image_resize");
    configure_item(get_item(cid, svg_image_tag+"image"), {
        preserveAspectRatio: constrain == 1 ? "xMinYMin meet" : "none",
        width: w,
        height: h
    }); 
}

function gui_ggee_image_toggle_visible(cid, svg_image_tag, visible) {
    //post("gui_ggee_image_toggle_visible " + visible);
    if (patchwin[cid])
    {
        configure_item(get_item(cid, svg_image_tag+"image"), {
            visibility: (visible ? "inherit" : "hidden")
        });
    }    
}

function gui_ggee_image_alpha(cid, svg_image_tag, alpha) {
    configure_item(get_item(cid, svg_image_tag+"image"), {
        opacity: alpha
    });    
}

function gui_ggee_image_rotate(cid, svg_image_tag, angle, x, y) {
    //post("gui_ggee_image_rotate " + angle + " " + svg_image_tag+"image");
    gui(cid).get_elem(svg_image_tag+"image", function(e) {
        if (pd_cache.get(svg_image_tag).type === "svg") {
            //post("...is svg");
            var size = pd_cache.get_svg_bbox(svg_image_tag);
            var scaleX = e.getAttribute("width") / size[2];
            var scaleY = e.getAttribute("height") / size[3];
            /*
            post("scale " + scaleX + " " + scaleY + "\n" + 
                (e.getAttribute("transform") !== null ?
                    e.getAttribute("transform").split(" ")[0] : "") +
                " scale(" + scaleX + " " + scaleY + ") translate(" +
                -size[0] + " " + -size[1] + ")"
                );
            */
            e.setAttribute("transform",
                "rotate(" + angle + "," + x + "," + y + ")" +
                " scale(" + scaleX + " " + scaleY + ")" +
                " translate(" + -size[0] + " " + -size[1] + ")");
        } else {
            e.setAttribute("transform",
                "rotate(" + angle + "," + x + "," + y + ")");
        }
    });
}

function gui_drawimage_vis(cid, x, y, obj, data, seqno, parent_tag) {
    gui(cid).get_elem(parent_tag) // main <g> within the scalar
    .append(function(frag) {
        var item,
            image_array = pd_cache.get(obj),
            len = image_array.length,
            i,
            image_container,
            xy_container,
            obj_tag = "draw" + obj.slice(1) + "." + data.slice(1);
        if (len < 1) {
            return;
        }
        // Wrap around for out-of-bounds sequence numbers
        if (seqno >= len || seqno < 0) {
            seqno %= len;
        }
        // Since sprites can have lots of images we don't want to
        // set props on each one of them every time the user changes
        // an attribute's value. So we use a "g" to receive all the
        // relevant changes.
        // Unfortunately "g" doesn't have x/y attys. So it can't propagate
        // those values down to the child images. Thus we have to add
        // another "g" as a parent and manually convert x/y changes
        // the user makes to a transform. (And we can't use the inner g's
        // transform because the user can set their own transform there.)
        xy_container = create_item(cid, "g", {
            id: obj_tag + "xy",
            transform: "translate(" + x + "," + y + ")"
        });
        image_container = create_item(cid, "g", {
            id: obj_tag
        });
        for (i = 0; i < len; i++) {
            item = create_item(cid, "image", {
                x: x,
                y: y,
                id: obj_tag + i,
                visibility: seqno === i ? "visible" : "hidden",
                preserveAspectRatio: "xMinYMin meet"
            });
            item.setAttributeNS("http://www.w3.org/1999/xlink", "href",
                "data:image/" + image_array[i].type + ";base64," +
                 image_array[i].data);
            image_container.appendChild(item);
        }
        xy_container.appendChild(image_container);
        frag.appendChild(xy_container);
        // Hack to set correct width and height
        for (i = 0; i < len; i++) {
            img_size_setter(cid, obj_tag+i, pd_cache.get(obj)[i].type,
                pd_cache.get(obj)[i].data);
        }
        return frag;
    });
}

// Hack
function gui_drawimage_xy(cid, obj, data, x, y) {
    var obj_tag = "draw" + obj.slice(1) + "." + data.slice(1);
    gui(cid).get_elem(obj_tag + "xy", {
        transform: "translate(" + x + "," + y + ")"
    });
}

function gui_drawimage_index(cid, obj, data, index) {
    var obj_tag = "draw" + obj.slice(1) + "." + data.slice(1);
    gui(cid).get_elem(obj_tag, function(image_container) {
        var len = image_container.childNodes.length,
            image = image_container.childNodes[((index % len) + len) % len],
            last_image =
                image_container.querySelectorAll('[visibility="visible"]'),
            i;
        for (i = 0; i < last_image.length; i++) {
            configure_item(last_image[i], { visibility: "hidden" });
        }
        configure_item(image, { visibility: "visible" });
    });
}

// Default png image data
function get_default_png_data() {
    return ["iVBORw0KGgoAAAANSUhEUgAAABkAAAAZCAMAAADzN3VRAAAAb1BMVEWBgYHX19",
            "f///8vLy/8/Pzx8PH3+Pf19fXz8/Pu7u7l5eXj4+Pn5+fs7Oza2tr6+vnq6urh",
            "4eHe3t7c3Nza2dr6+fro6Og1NTXr6+xYWFi1tbWjo6OWl5aLjItDQ0PPz8+/v7",
            "+wsLCenZ5zc3NOTk4Rpd0DAAAAqElEQVQoz62L2Q6CMBBFhcFdCsomq+v/f6Mn",
            "bdOSBn3ypNO5Nyez+kG0zN9NWZZK8RRbB/2XmMLSvSZp2mehTMVcLGIYbcWcLW",
            "1/U4PIZCvmOCMSaWzEHGaMIq2NmJNn4ORuMybP6xxYD0SnE4NJDdc0fYv0LCJg",
            "9g4RqV3BrJfB7Bzc+ILZOjC+YDYOjC+YKqsyHlOZAX5Msgwm1iRxgDYBSWjCm+",
            "98AAfDEgD0K69gAAAAAElFTkSuQmCC"
           ].join("");
}

function gui_load_default_image(dummy_cid, key) {
    pd_cache.set(key, {
        type: "png",
        data: get_default_png_data()
    });
}

// Load an image and cache the base64 data
// ico@vt.edu 2021-10-19:
// objtype is reserved to recognize previously batch-loaded images by
// different objects, so that when the last of that kind of the object
// has been deleted, we can dispose of all such images
function gui_load_image(cid, key, filepath, objtype) {
    //post("gui_load_image " + filepath + " " + key);
    // ico 2023-11-15:
    // since we now have both image for displaying images,
    // and g (group) for displaying svg content, we need to
    // check if we need to clear group and swap ids, as
    // both are supposed to share the same id, but since
    // ids are supposed to be unique, we prepend D (for
    // disabled) to the one that is not being used. then,
    // when loading a new file, here we check what needs to
    // be updated and renamed, accordingly.
    var ext = path.extname(filepath),
        data = fs.readFileSync(filepath,
            (ext.slice(1) === "svg" ? "" : "base64"));
    if (ext.slice(1) === "svg") {
        // we are loading svg
        //post("SVG searching for " + key + "Dimage");
        gui(cid).get_elem(key+"gobj", function(e) {
            var f = e.querySelector("#" + key + "Dimage");
            var g = e.querySelector("#" + key + "image");
            if (f !== null && f.tagName === "g") {
                //post("...whoa");
                f.style.setProperty("visibility", "inherit");
                f.style.setProperty("display", "none");
                f.id = key + "image";
                g.style.setProperty("visibility", "hidden");
                g.style.setProperty("display", "none");
                g.id = key + "Dimage";
            }
        });
    } else {
        // we are loading regular image
        //post("REGULAR searching for " + key + "Dimage");
        gui(cid).get_elem(key+"gobj", function(e) {
            var f = e.querySelector("#" + key + "Dimage");
            var g = e.querySelector("#" + key + "image");
            if (f !== null && f.tagName === "image") {
                //post("...whoa");
                f.style.setProperty("visibility", "inherit");
                f.style.setProperty("display", "none");
                f.id = key + "image";
                g.innerHTML = "";
                g.style.setProperty("visibility", "hidden");
                g.style.setProperty("display", "none");
                g.id = key + "Dimage";
            }
        });
    }
    //post("gui_load_image data=" + data);
    pd_cache.set(key, {
        type: ext === ".jpeg" ? "jpg" : ext.slice(1),
        data: data,
        creator: (objtype ? objtype : 0)
    });
}

// Load an image and cache the base64 data then send a callback with its size
function gui_moonlib_load_image(cid, key, filepath, callback, objtype) {
    //post("gui_moonlib_load_image " + filepath);
    var data = fs.readFileSync(filepath,"base64"),
        ext = path.extname(filepath);
    pd_cache.set(key, {
        type: ext === ".jpeg" ? "jpg" : ext.slice(1),
        data: data,
        creator: (objtype ? objtype : 0) 
    });
    var img = new pd_window.Image(); // create an image in the pd_window context
    img.onload = function() {
        pdsend(callback, "_imagesize", this.width, this.height);
    };
    img.src = "data:image/" + pd_cache.get(key).type +
        ";base64," + pd_cache.get(key).data;
}

// Draw an image in an object-- used for ggee/image, moonlib/image and
// tof/imagebang. For the meaning of tk_anchor see img_size_setter. This
// interface assumes there is only one image per gobject. If you try to
// set more you'll get duplicate ids.
// ico@vt.edu 2020-11-17: added requested width, height, and constrain
// aspect ratio which is used by the ggee/image, and in addition the
// image type (0 = ggee, 1 = moonlib)
// ico@vt.edu 2021-03-30: added isgop which is used only for ggee/image
// to toggle visibility off before the image is properly positioned through
// a callback. this removes flicker.
function gui_gobj_draw_image(cid, tag, image_key, tk_anchor, w, h, constrain, type, isgop) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        //post("gui_gobj_draw_image type=" + type + " filetype=" +
        //    pd_cache.get(image_key).type + " isgop=" + isgop);
        if (pd_cache.get(image_key).type !== "svg") {
            var i = create_item(cid, "image", {
                id: tag + "image",
                preserveAspectRatio: constrain ? "xMinYMin meet" : "none",
                type: type
            });
            var j = create_item(cid, "g", {
                id: tag + "Dimage",
                class: "graphsvg",
                type: type
            });
            i.style.setProperty("display", "none");
            j.style.setProperty("display", "none");
            j.style.setProperty("visibility", "hidden");
            i.setAttributeNS("http://www.w3.org/1999/xlink", "href",
                "data:image/" + pd_cache.get(image_key).type + ";base64,"
                + pd_cache.get(image_key).data);
            if (type === 0) {
                img_size_resizable_setter(cid, tag+"image",
                    pd_cache.get(image_key).type, pd_cache.get(image_key).data,
                    tk_anchor, w, h, constrain);
            } else {
                img_size_setter(cid, tag+"image", pd_cache.get(image_key).type,
                    pd_cache.get(image_key).data, tk_anchor);
            }
            frag.appendChild(j);
            frag.appendChild(i);
            return frag;
        } else {
            // this is an svg image
            var size = pd_cache.get(image_key).get_svg_size;
            var scale;
            if (size !== undefined) {
                scale = "scale(" + (w / size[0]) + " " + (h / size[1]) + ")";
            } else {
                scale = "scale(1 1)";
            }

            var i = create_item(cid, "g", {
                id: tag + "image",
                class: "graphsvg",
                type: type
            });
            var j = create_item(cid, "image", {
                id: tag + "Dimage",
                preserveAspectRatio: constrain ? "xMinYMin meet" : "none",
                type: type
            });
            i.style.setProperty("display", "none");
            j.style.setProperty("display", "none");
            j.style.setProperty("visibility", "hidden");
            //i.setAttribute("xmlns", "http://www.w3.org/2000/svg");
            //i.style.setProperty("width", w);
            //i.style.setProperty("height", h);
            // set the svg data and get rid of the outer <svg> tag
            i.innerHTML = pd_cache.get(image_key).data;
            var svg = i.querySelector('svg');
            svg.outerHTML = svg.innerHTML;
            frag.appendChild(i);
            frag.appendChild(j);
            return frag;
        }
    });
}

// ico@vt.edu 2021-03-25:
// We use this to remove flicker due to asynchronous loading of image and its
// potential repositioning due to ggee's gop_spill option, so we initially
// make the image invisible in the gui_gobj_draw_image, and make it visible
// only after we know the image has successfully loaded. See image_drawme
// and image_open functions call inside ggee/image.c file.
function gui_ggee_image_display(cid, tag, vis) {
    // check if the image exists yet
    //post("gui_ggee_image_display vis=" + vis +
    //    " " + tag +
    //    " check=" + get_item(cid, tag+"image"));
    if (check_cid(cid)) {
        if (get_item(cid, tag+"image") !== null) {
            if (vis === 1) {
                /*
                configure_item(get_item(cid, tag+"image"), {
                    display: "block"
                });
                */
                get_item(cid, tag+"image").
                    style.setProperty("display", "block");
            } else if (vis === 0) {
                /*
                configure_item(get_item(cid, tag+"image"), {
                    display: "none"
                });
                */
                get_item(cid, tag+"image").
                    style.setProperty("display", "none");
            }
        }
    }
}

// ico@vt.edu 2020-11-13:
// Special function just for ggee/image designed to deal with the
// gop_spill option while supporting the newly introduced compliance
// with the xpix and ypix corresponding to the true top-left corner
// of the object's clickable area
function gui_ggee_image_offset(cid, tag, image_key, xoffset, yoffset) {
    //post("gui_ggee_image_offset " + pd_cache.get(image_key).type +
    //    " " + tag + "image" + " key=" + image_key);
    if (pd_cache.get(image_key).type !== "svg") {
        configure_item(get_item(cid, tag+"image"), {
            x: xoffset,
            y: yoffset
        });
    } else {
        // svg
        gui(cid).get_elem(tag+"image", function(e) {
            var size = pd_cache.get_svg_bbox(image_key);
            //post("size=" + size.length + " " + size[0]);
            //post("width=" + e.getAttribute("width") +
            //    " height=" + e.getAttribute("height"));

            //post("..." + e.getAttribute("transform"));
            var scaleX = e.getAttribute("width") / size[2];
            var scaleY = e.getAttribute("height") / size[3];
            //post("...transform=" + e.getAttribute("transform"));

            e.setAttribute("transform",
                (e.getAttribute("transform") !== null ?
                    e.getAttribute("transform").split(" ")[0] : "") +
                " scale(" + scaleX + " " + scaleY + ") translate(" +
                -size[0] + " " + -size[1] + ")");
        });
    }
}

function gui_image_size_callback(cid, key, callback) {
    //post("gui_image_size_callback " + pd_cache.get(key).type);
    if (pd_cache.get(key).type !== "svg") {
        var img = new pd_window.Image(); // create an image in the pd_window context
        img.onload = function() {
            //post("..." + this.width + "," + this.height);
            pdsend(callback, "_imagesize", this.width, this.height);
        };
        img.src = "data:image/" + pd_cache.get(key).type +
            ";base64," + pd_cache.get(key).data;
    } else {
        // sending svg info
        // ico 2023-11-15: this is a painful way of getting
        // default svg image size...
        //post("gui_image_size_callback svg");
        gui(cid).get_elem("svg_sizing", function(e) {
            e.innerHTML = pd_cache.get(key).data;
            var img = e.querySelector("svg");
            var size = img.getBBox();
            /*
            post("..." +
                Math.floor(size.x) + " " +
                Math.floor(size.y) + " " +
                Math.floor(size.width) + " " +
                Math.floor(size.height));
            */
            pd_cache.set_svg_bbox(
                key, size.x, size.y, size.width, size.height);
            //post("sending _imagesize");
            pdsend(callback, "_imagesize",
                Math.floor(size.width), Math.floor(size.height));
            e.innerHTML = "";
        });
    }
}

function gui_image_draw_border(cid, tag, x, y, w, h/*, imgw, imgh*/, onoff) {
    if (onoff == 0) {
        gui(cid).get_gobj(tag)
        .q("path", function(border) {
            border.parentNode.removeChild(border);
        });
    } else {
        gui(cid).get_gobj(tag)
        .append(function(frag) {
            var b = create_item(cid, "path", {
                "stroke-width": "1",
                fill: "none",
                d: ["m", 0, 0, w, 0,
                    "m", 0, 0, 0, h,
                    "m", 0, 0, -w, 0,
                    "m", 0, 0, 0, -h
                   ].join(" "),
                visibility: "inherit",
                class: "image border"
            });
            frag.appendChild(b);
            return frag;
        });
    }
}

function gui_moonlib_image_draw_border(cid, tag, x, y, w, h, onoff) {
    //post("gui_moonlib_image_draw_border "+x+" "+y+" "+w+" "+h);
    if (onoff == 0) {
        gui(cid).get_gobj(tag)
        .q("path", function(border) {
            border.parentNode.removeChild(border);
        });
    } else {
        gui(cid).get_gobj(tag)
        .append(function(frag) {
            var b = create_item(cid, "path", {
                "stroke-width": "1",
                fill: "none",
                d: ["m", -w/2, -h/2, w, 0,
                    "m", 0, 0, 0, h,
                    "m", 0, 0, -w, 0,
                    "m", 0, 0, 0, -h
                   ].join(" "),
                visibility: "inherit",
                class: "image border"
            });
            frag.appendChild(b);
            return frag;
        });
    }
}

function gui_image_toggle_border(cid, tag, state) {
    gui(cid).get_gobj(tag)
    .q(".border", {
        visibility: state === 0 ? "hidden" : "inherit"
    });
}

function gui_image_update_border(cid, tag, w, h) {
    //post("gui_image_update_border");
    gui(cid).get_gobj(tag, function(e) {
        var a = e.querySelectorAll(".image.border");
        configure_item(a[0], {
            d: ["m", 0, 0, w, 0,
                "m", 0, 0, 0, h,
                "m", 0, 0, -w, 0,
                "m", 0, 0, 0, -h
               ].join(" "),
        });
    });
}

// Switch the data for an existing svg image
// this is shared by moonlib and ggee
function gui_image_configure(cid, tag, image_key, tk_anchor) {
    //post("gui_image_configure");
    gui(cid).get_elem(tag+"image", function(e) {
        if (pd_cache.get(image_key)) {
            if (pd_cache.get(image_key).type !== "svg") {
                //post("...not svg");
                e.setAttributeNS("http://www.w3.org/1999/xlink", "href",
                    "data:image/" + pd_cache.get(image_key).type + ";base64," +
                     pd_cache.get(image_key).data);
                img_size_setter(cid, tag+"image", pd_cache.get(image_key).type,
                    pd_cache.get(image_key).data, tk_anchor);
            } else {
                // set the svg data and get rid of the outer <svg> tag
                e.innerHTML = pd_cache.get(image_key).data;
                var svg = e.querySelector('svg');
                svg.outerHTML = svg.innerHTML;
            }
        } else {
            // need to change this to an actual error
            post("image: error: can't find image");
        }
    });
}

// Switch the data for an existing svg image
function gui_image_configure_preloaded(cid, tag, image_key, tk_anchor, w, h, constrain) {
    gui(cid).get_elem(tag+"image", function(e) {
        if (pd_cache.get(image_key)) {
            e.setAttributeNS("http://www.w3.org/1999/xlink", "href",
                "data:image/" + pd_cache.get(image_key).type + ";base64," +
                 pd_cache.get(image_key).data);
            //img_size_setter(cid, tag+"image", pd_cache.get(image_key).type,
            //    pd_cache.get(image_key).data, tk_anchor);
        } else {
            // need to change this to an actual error
            post("image: error: can't find image");
        }
    });
    configure_item(get_item(cid, tag+"image"), {
        preserveAspectRatio: constrain == 1 ? "xMinYMin meet" : "none",
        width: w,
        height: h,
        x: tk_anchor === "center" ? 0 - w/2 : 0,
        y: tk_anchor === "center" ? 0 - h/2 : 0
    }); 
}

// Move an image
function gui_image_coords(cid, ownercid, parentcid, tag, x, y, is_toplevel) {
    //post("gui_image_coords x=" + x + " y=" + y + " is_toplevel=" + is_toplevel);
    // ggee/image accepts a message that can trigger this, meaning
    // [loadbang] can end up calling this before the patchwindow exists.
    // So we have to check for existence below
    var draw_xpos, draw_ypos;
    draw_xpos = x;
    draw_ypos = y;
    if (is_toplevel === 0) {
        gui(cid).get_elem("patchsvg", function(svg_elem, w) {
            var tgt = w.document.getElementsByClassName(ownercid + "svg");
            var svg_view_box = svg_elem.getAttribute("viewBox").split(" ");
            draw_xpos = draw_xpos - tgt[0].getScreenCTM().e +
                svg_elem.getBoundingClientRect().left - svg_view_box[0];
            draw_ypos = draw_ypos - tgt[0].getScreenCTM().f +
                svg_elem.getBoundingClientRect().top - svg_view_box[1];
        });
    }
    draw_xpos += 0.5;
    draw_ypos += 0.5;
    //post("...gui_image_coords offset x=" + draw_xpos + " y=" + draw_ypos);
    gui(cid).get_gobj(tag, function(e) {
        elem_move(e, draw_xpos, draw_ypos);
    });
}

// Scope~
function gui_scope_draw_bg(cid, tag, fg_color, bg_color, w, h, grid_width, dx, dy, is_toplevel) {
    //post("gui_scope_draw_bg istoplevel=" + is_toplevel);
    gui(cid)
    .get_gobj(tag)
    .append(function(frag) {
        var bg = create_item(cid, "rect", {
            width: w,
            height: h,
            fill: bg_color,
            class: "bg " + (!!is_toplevel ? "toplevel " : "") + "border",
            stroke: "black",
            "stroke-width": grid_width
        }),
        path,
        path_string = "",
        fg_xy_path, // to be used for the foreground lines
        fg_mono_path,
        i, x, y, align_x, align_y;
        // Path strings for the grid lines
        // vertical lines...
        for (i = 0, x = dx; i < 7; i++, x += dx) {
            align_x = (x|0) === x ? x : Math.round(x);
            path_string += ["M", align_x, 0, "V", h].join(" ");
        }
        // horizontal lines...
        for (i = 0, y = dy; i < 3; i++, y += dy) {
            align_y = (y|0) === y ? y : Math.round(y);
            path_string += ["M", 0, align_y, "H", w].join(" ");
        }
        path = create_item(cid, "path", {
            d: path_string,
            fill: "none",
            stroke: "black",
            "stroke-width": grid_width,
        });
        // We go ahead and create a path to be used in the foreground. We'll
        // set the actual path data in the draw/redraw functions. Doing it this
        // way will save us having to create and destroy DOM objects each time
        // we redraw the foreground
        fg_xy_path = create_item(cid, "path", {
            fill: "none",
            stroke: fg_color,
            class: "fgxy"
        });
        fg_mono_path = create_item(cid, "path", {
            fill: "none",
            stroke: fg_color,
            class: "fgmono"
        });
        frag.appendChild(bg);
        frag.appendChild(path);
        frag.appendChild(fg_xy_path);
        frag.appendChild(fg_mono_path);
        return frag;
    });
}

function scope_configure_fg(cid, tag, type, data_array) {
    gui(cid)
        .get_gobj(tag)
        .q(type, { // class ".fgxy" or ".fgmono"
            d: data_array.join(" ")
    });
}

function gui_scope_configure_fg_xy(cid, tag, data_array) {
    scope_configure_fg(cid, tag, ".fgxy", data_array);
}

function gui_scope_configure_fg_mono(cid, tag, data_array) {
    scope_configure_fg(cid, tag, ".fgmono", data_array);
}

function gui_scope_configure_bg_color(cid, tag, color) {
    gui(cid).get_gobj(tag)
        .query(".bg", {
            fill: color
        });
}

function gui_scope_configure_fg_color(cid, tag, color) {
    gui(cid).get_gobj(tag)
        .q(".fgxy", { stroke: color })
        .q(".fgmono", { stroke: color });
}

function gui_scope_clear_fg(cid, tag) {
    scope_configure_fg(cid, tag, ".fgxy", []);
    scope_configure_fg(cid, tag, ".fgmono", []);
}

// unauthorized/grid

function get_grid_data(w, h, x_l, y_l) {
    var d, x, y, offset;
    d = [];
    offset = Math.floor(w / x_l);
    if (offset > 0) {
        for (x = 0; x < w; x += offset) {
            d = d.concat(["M", x, 0, x, h]); // vertical line
        }
    } else {
        post("Warning: too many gridlines");
    }
    offset = Math.floor(h / y_l);
    if (offset > 0) {
        for (y = 0; y < h; y += offset) {
            d = d.concat(["M", 0, y, w, y]); // horizontal line
        }
    } else {
        post("Warning: too many gridlines");
    }
    return d.join(" ");
}

function gui_configure_grid(cid, tag, w, h, bg_color, has_grid, x_l, y_l) {
    var grid_d_string = !!has_grid ? get_grid_data(w, h, x_l, y_l) : "",
        point_size = 5;
    gui(cid).get_gobj(tag)
    .q(".bg", {
        width: w,
        height: h,
        fill: bg_color,
    })
    .q(".border", {
        d: ["M", 0, 0, w, 0,
            "M", 0, h, w, h,
            "M", 0, 0, 0, h,
            "M", w, 0, w, h
           ].join(" "),
        fill: "none",
        stroke: "black",
        "stroke-width": 1
    })
    .q(".out_0", {
        y: h + 1,
        width: 7,
        height: 1,
        fill: "none",
        stroke: "black",
        "stroke-width": 1
    })
    .q(".out_1", {
        x: w - 7,
        y: h + 1,
        width: 7,
        height: 1,
        fill: "none",
        stroke: "black",
        "stroke-width": 1
    })
    .q(".grid", {
        d: grid_d_string,
        stroke: "white",
        "stroke-width": 1
    })
    .q(".point", {
        style: "visibility: none;",
        width: 5,
        height: 5,
        fill: "#ff0000",
        stroke: "black",
        "stroke-width": 1
    });
}

function gui_grid_new(cid, ownercid, parentcid, tag, x, y, is_toplevel) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        gui_gobj_new(cid, ownercid, parentcid, tag, "obj wide-select-border", x, y, is_toplevel, 0, "grid", "grid");
    });
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var bg = create_item(cid, "rect", {
            class: "bg"
        }),
        border = create_item(cid, "path", {
            class: (!!is_toplevel ? "toplevel " : "") + "border" // now we can inherit the css border styles
        }),
        out_0 = create_item(cid, "rect", {
            class: "out_0",
            style: "display: " + (is_toplevel ? "inline;" : "none;")
        }),
        out_1 = create_item(cid, "rect", {
            class: "out_1",
            style: "display: " + (is_toplevel ? "inline;" : "none;")
        }),
        grid = create_item(cid, "path", {
            class: "grid"
        }),
        point = create_item(cid, "rect", {
            class: "point"
        });
        frag.appendChild(bg);
        frag.appendChild(out_0);
        frag.appendChild(out_1);
        frag.appendChild(grid);
        frag.appendChild(point);
        frag.appendChild(border);
        return frag;
    });
}

function gui_grid_point(cid, tag, x, y) {
    gui(cid).get_gobj(tag)
    .q(".point", {
        x: x,
        y: y,
        style: "visibility: inherit;"
    });
}

// unauthorized/pianoroll
function pianoroll_get_id(tag, type, i, j) {
    // Because i and j are just integers we want to prevent ambiguity.
    // For example, "1" and "23" concatenate the same as "12" and "3". So
    // we separate the two with an underscore.
    return tag + "_" + type + "_" + i + "_" + j;
}

function gui_pianoroll_draw_rect(cid, tag, x1, y1, x2, y2, i, j, type) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var r = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            id: pianoroll_get_id(tag, type, i, j),
            fill: type === "pitch" ? "#771623" : "#562663",
            stroke: "#998121",
            "stroke-width": "1"
        });
        frag.appendChild(r);
        return frag;
    });
}

// consider doing a single call with an array of data here...
function gui_pianoroll_update_rect(cid, tag, type, i, j, fill) {
    gui(cid)
    .get_elem(pianoroll_get_id(tag, type, i, j), {
        fill: fill
    });
}

// just clear out everything inside the container
function gui_pianoroll_erase_innards(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        e.innerHTML = "";
    });
}

// pd-lua gfx helpers (aggraef@gmail.com)

// create the graphics container (a gobj)
function gui_luagfx_new(cid, tag, xpos, ypos, is_toplevel) {
    gui_gobj_new(cid, cid, cid, tag, "obj", xpos, ypos, is_toplevel, 0, "pdlua", "graphical pdlua object");
}

// create a graphics layer (pd-lua 0.12.19)
function gui_luagfx_new_layer(cid, tag, layer_tag) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var layer = create_item(cid, "g", { id: layer_tag + "gobj" });
        frag.appendChild(layer);
        return frag;
    });
}

// clear the contents of the graphics container (or layer, per pd-lua 0.12.19)
function gui_luagfx_clear(cid, tag) {
    // get rid of all contents
    gui(cid).get_gobj(tag, function(g) {
        g.innerHTML = "";
    });
}

// this erases the gobj (completely removes it)
function gui_luagfx_erase(cid, tag) {
    gui_gobj_erase(cid, tag);
}

// a bunch of drawing operations as required by the pd-lua graphics interface
function gui_luagfx_fill_all(cid, tag, gfxtag, color, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        // border rectangle in the usual style with the given bg color
        var r = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            class: "border",
            style: "fill: "+color+";",
            id: gfxtag
        });
        frag.appendChild(r);
        return frag;
    });
}

function gui_luagfx_fill_rect(cid, tag, gfxtag, color, width, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            fill: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_stroke_rect(cid, tag, gfxtag, color, width, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            fill: "none",
            stroke: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_fill_rounded_rect(cid, tag, gfxtag, color, width, x1, y1, x2, y2, rx, ry) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            rx: rx,
            ry: ry,
            fill: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_stroke_rounded_rect(cid, tag, gfxtag, color, width, x1, y1, x2, y2, rx, ry) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "rect", {
            x: x1,
            y: y1,
            width: x2 - x1,
            height: y2 - y1,
            rx: rx,
            ry: ry,
            fill: "none",
            stroke: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_fill_ellipse(cid, tag, gfxtag, color, width, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "ellipse", {
            cx: (x2 - x1) * 0.5 + x1,
            cy: (y2 - y1) * 0.5 + y1,
            rx: (x2 - x1) * 0.5,
            ry: (y2 - y1) * 0.5,
            fill: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_stroke_ellipse(cid, tag, gfxtag, color, width, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "ellipse", {
            cx: (x2 - x1) * 0.5 + x1,
            cy: (y2 - y1) * 0.5 + y1,
            rx: (x2 - x1) * 0.5,
            ry: (y2 - y1) * 0.5,
            fill: "none",
            stroke: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_draw_line(cid, tag, gfxtag, color, width, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "line", {
            x1: x1,
            y1: y1,
            x2: x2,
            y2: y2,
            stroke: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_draw_text(cid, tag, gfxtag, color, width, font_height, x, y, text) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        // Check gui_text_new for some black magic being used here.
        var gx = create_item(cid, "text", {
            transform: "translate(" + x + ")",
            y: y + font_height,
            width: width,
            "font-size": font_height + "px",
            fill: color,
            id: gfxtag
        });
        // fill svg_text with tspan content by splitting on "\n"
        text_to_tspans(cid, gx, text);
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_fill_path(cid, tag, gfxtag, color, width, path) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "path", {
            d: path.join(" "),
            fill: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

function gui_luagfx_stroke_path(cid, tag, gfxtag, color, width, path) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var gx = create_item(cid, "path", {
            d: path.join(" "),
            fill: "none",
            stroke: color,
            "stroke-width": width,
            id: gfxtag
        });
        frag.appendChild(gx);
        return frag;
    });
}

// mknob from moonlib
function gui_mknob_new(cid, ownercid, parentcid, tag, x, y, is_toplevel, show_in, show_out,
    is_footils_knob) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        gui_gobj_new(cid, ownercid, parentcid, tag, "obj", x, y, is_toplevel, 0, "knob", "knob");
    });
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var border = create_item(cid, "path", {
            // now we can inherit the css border styles
            class: (!!is_footils_knob ? "flatgui knob " : "") + 
                (!!is_toplevel ? "toplevel " : "") + "border"
        }),
        circle = create_item(cid, (!!is_footils_knob ? "path" : "circle"), {
                class: (!!is_footils_knob ? "circle" : "")
        }),
        line = create_item(cid, "line", {
            class: "dial " + tag + "dial"
        });
        frag.appendChild(border);
        frag.appendChild(circle);
        /* An extra circle for footils/knob */
        if (!!is_footils_knob) {
            frag.appendChild(create_item(cid, "path", {
                class: "dial_frag " + tag + "dial_frag"
            }));
        }
        frag.appendChild(line);
        return frag;
    });
}

// ico@vt.edu 20200923: improved drawing of the flatgui/knob
// code adapted from https://jsbin.com/fiqulevevu/edit?html,js,output
function polarToCartesian(centerX, centerY, radius, angleInDegrees) {
  var angleInRadians = (angleInDegrees-90) * Math.PI / 180.0;

  return {
    x: centerX + (radius * Math.cos(angleInRadians)),
    y: centerY + (radius * Math.sin(angleInRadians))
  };
}

function describeArc(x, y, radius, startAngle, endAngle){
    var start = polarToCartesian(x, y, radius, endAngle);
    var end = polarToCartesian(x, y, radius, startAngle);

    var largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";
    var d = [
        "M", start.x, start.y, 
        "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y
    ].join(" ");

    return d;       
}

function gui_configure_mknob(cid, tag, size, bg_color, fg_color,
    is_footils_knob, dial_w, circle_w, frag_w) {
    // non-footils knob uses dial_w = 2 and circle_w = 1
    // footils uses dial_w = 2 circle_w = 3 and frag_w = 4
    var xtra_w = 0;
    if (is_footils_knob) {
        xtra_w = circle_w;
        if (frag_w > xtra_w) xtra_w = frag_w;
        xtra_w = xtra_w / 2;
        if (xtra_w < 0) xtra_w = 0;
    }
    var w = size,
        h = size;
    //post("gui_configure_mknob size=" + size + " w=" + xtra_w);

    var g = gui(cid).get_gobj(tag)
    .q(".border", {
        d: ["M", -xtra_w, -xtra_w, w+xtra_w, -xtra_w,
            "M", -xtra_w, h+xtra_w, w+xtra_w, h+xtra_w,
            "M", -xtra_w, -xtra_w, -xtra_w, h+xtra_w,
            "M", w+xtra_w, -xtra_w, w+xtra_w, h+xtra_w
           ].join(" "),
        fill: "none"
    })
    .q("." + tag + "dial", { // indicator
        "stroke-width": dial_w,
        stroke: fg_color
    });

    if (!is_footils_knob) {
        g.q("circle", {
            cx: size / 2,
            cy: size / 2,
            r: size / 2,
            fill: bg_color,
            stroke: "black",
            "stroke-width": 1,
            "stroke-dasharray": "none",
            "stroke-dashoffset": "0"
        });
    } else {
        g.q(".circle", {
            stroke: "black",
            fill: "none",
            "stroke-width": circle_w,
            "d": describeArc(size/2, size/2, size/2 - 1, 193, 528)
        });
        g.q("." + tag + "dial_frag", {
            "knob_w": size,
            fill: "none",
            stroke: bg_color,
            "stroke-width": frag_w,
            "d": describeArc(size/2, size/2, size/2 - 1, 192.9, 528.1),
        });
    }
}

function gui_turn_mknob(cid, tag, x1, y1, x2, y2, is_footils_knob, val) {
    var g = gui(cid).get_gobj(tag)
    .q("." + tag + "dial", { // indicator
        x1: x1,
        y1: y1,
        x2: x2,
        y2: y2
    });
    if (!!is_footils_knob) {
        // this is stupid but apparently the only way to get an attribute
        // is there a better way using .q() or g.q()?
        var size;
        gui(cid).get_elem("patchsvg", function(svg_elem, w) {
            var dial = w.document.getElementsByClassName(tag + "dial_frag");
            size = dial[0].getAttribute("knob_w");
        });
        /*while (val > 1) {
            val -= 1;
        }
        while (val < 0) {
            val += 1;
        }*/
        g.q("." + tag + "dial_frag", {
            "d": describeArc(size/2, size/2, size/2 - 1, 193, 193 + val * (528-193))
        });
    }
}

// room_sim_2d and room_sim_3d objects from iemlib
function gui_room_sim_new(cid, ownercid, parentcid, tag, x, y, w, h, is_toplevel) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        gui_gobj_new(cid, ownercid, parentcid, tag,
            "obj wide-select-border", x, y, is_toplevel, 0, "room_sim", "room_sim");
    });
    gui(cid).get_gobj(tag)
    .append(function(frag) {
//        frag.appendChild(line);
        return frag;
    });
}

function gui_room_sim_map(cid, tag, w, h, rad, head,
    xpix, ypix, fontsize, fcol, bcol, src_array, r3d) {
    gui(cid).get_gobj(tag, function(e) {
        gui_text_draw_border(cid, tag, 0, 0, w, h, 0);
        // Set the style for the background directly... otherwise the
        // default theme bgcolor will be used
        e.querySelector(".border").style.fill = bcol;
    })
    .append(function(frag) {
        var x1 = xpix - rad,
            x2 = xpix + rad - 1,
            y1 = ypix - rad,
            y2 = ypix + rad - 1,
            dx = -((rad * Math.sin(head * 0.0174533) + 0.49999)|0),
            dy = -((rad * Math.cos(head * 0.0174533) + 0.49999)|0),
            i,
            text;
        for (i = 0; i < src_array.length; i++) {
            text = create_item(cid, "text", {
                x: src_array[i][0],
                y: src_array[i][1],
                fill: src_array[i][2],
                "font-size": fontsize,
                "dominant-baseline": "middle"
            });
            text.textContent = (i + 1).toString();
            frag.appendChild(text);
        }
        var ellipse = create_item(cid, "ellipse", {
            cx: (x2 - x1) * 0.5 + x1,
            cy: (y2 - y1) * 0.5 + y1,
            rx: (x2 - x1) * 0.5,
            ry: (y2 - y1) * 0.5,
            "stroke-width": 1,
            "stroke": fcol,
            "fill": "none"
        }),
        ellipse2 = create_item(cid, "ellipse", {
            // for room_sim_3d
            cx: r3d ? (r3d[2] - r3d[0]) * 0.5 + r3d[0] : 0,
            cy: r3d ? (r3d[3] - r3d[1]) * 0.5 + r3d[1] : 0,
            rx: r3d ? (r3d[2] - r3d[0]) * 0.5 : 0,
            ry: r3d ? (r3d[3] - r3d[1]) * 0.5 : 0,
            "stroke-width": 1,
            stroke: fcol,
            fill: "none"
        }),
        line = create_item(cid, "line", {
            x1: xpix,
            y1: ypix,
            x2: xpix + dx,
            y2: ypix + dy,
            "stroke-width": 3,
            stroke: fcol
        });
        frag.appendChild(ellipse);
        frag.appendChild(ellipse2);
        frag.appendChild(line);
        return frag;
    })
}

function gui_room_sim_update_src(cid, tag, i, x, y, font_size, col) {
    gui(cid).get_gobj(tag, function(e) {
        var a = e.querySelectorAll("text");
        if (a.length && i < a.length) {
            configure_item(a[i], {
                x: x,
                y: y,
                "font-size": font_size,
                fill: col
            });
        }
    });
}

function gui_room_sim_update(cid, tag, x0, y0, dx, dy, pixrad) {
    gui(cid).get_gobj(tag)
    .q("line", {
        x1: x0,
        y1: y0,
        x2: x0 + dx,
        y2: y0 + dy
    })
    .q("ellipse", {
        rx: ((x0 + pixrad - 1) - (x0 - pixrad)) * 0.5,
        ry: ((y0 + pixrad - 1) - (y0 - pixrad)) * 0.5,
        cx: ((x0 + pixrad - 1) - (x0 - pixrad)) * 0.5 + (x0 - pixrad),
        cy: ((y0 + pixrad - 1) - (y0 - pixrad)) * 0.5 + (y0 - pixrad),
    });
}

// for room_sim_3d
function gui_room_sim_head2(cid, tag, x1, y1, x2, y2) {
    gui(cid).get_gobj(tag, function(e) {
        configure_item(e.querySelectorAll("ellipse")[1], {
            rx: (x2 - x1) * 0.5,
            ry: (y2 - y1) * 0.5,
            cx: (x2 - x1) * 0.5 + x1,
            cy: (y2 - y1) * 0.5 + y1
        });
    });
}

function gui_room_sim_fontsize(cid, tag, i, size) {
    gui(cid).get_gobj(tag, function(e) {
        var i, a;
        a = e.querySelectorAll("text");
        if (a.length) {
            for (i = 0; i < a.length; i++) {
                configure_item(a[i], {
                    "font-size": size
                });
            }
        }
    });
}

// for the dial thingy
function gui_room_sim_colors(cid, tag, fg, bg) {
    gui(cid).get_gobj(tag)
    .q("ellipse", {
        stroke: fg
    })
    .q("line", {
        stroke: fg
    })
    .q(".border", function(e) {
        e.style.fill = bg;
    });
}

function gui_room_sim_erase(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        e.innerHTML = "";
    });
}

function add_popup(cid, popup) {
    popup_menu[cid] = popup;
}

// envgen
function gui_envgen_draw_bg(cid, tag, bg_color, w, h, is_toplevel, points_array) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var bg, border, pline;
        bg = create_item(cid, "rect", {
            width: w,
            height: h,
            fill: bg_color,
            stroke: "black",
            "stroke-width": "2",
            transform: "translate(0.5, 0.5)"
        });
        // draw an extra path so we can give envgen
        // a border class without affecting the
        // background color of envgen
        border = create_item(cid, "path", {
            "stroke-width": 1,
            d: ["M", 0, 0, w+1, 0,
                "M", w+1, 0, w+1, h+1,
                "M", w+1, h+1, 0, h+1,
                "M", 0, h+1, 0, 0].join(" "),
            "class": (is_toplevel ? "toplevel border" : "border")
        });
        pline = create_item(cid, "polyline", {
            stroke: "black",
            fill: "none",
            transform: "translate(2, 2)",
            points: points_array.join(" ")
        });
        frag.appendChild(bg);
        frag.appendChild(border);
        frag.appendChild(pline);
        return frag;
    });
}

function gui_envgen_draw_doodle(cid, tag, cx, cy) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var d = create_item(cid, "circle", {
            r: "2",
            cx: cx + 2,
            cy: cy + 2,
            class: "envgenpoint"
        });
        frag.appendChild(d);
        return frag;
    });
}

function gui_envgen_erase_doodles(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        var elem_array = e.querySelectorAll("circle"),
        i;
        if (elem_array.length > 0) {
            for (i = 0; i < elem_array.length; i++) {
                elem_array[i].parentNode.removeChild(elem_array[i]);
            }
        }
    });
}

function gui_envgen_coords(cid, tag, w, h, points_array) {
    gui(cid).get_gobj(tag)
    .q(".border", {
        d: ["M", 0, 0, w+1, 0,
            "M", w+1, 0, w+1, h+1,
            "M", w+1, h+1, 0, h+1,
            "M", 0, h+1, 0, 0].join(" ")
    })
    .q(".rect", {
        width: w,
        height: h
    })
    .q("polyline", {
        points: points_array.join(" ")
    });
}

function gui_envgen_text(cid, tag, x, y, value, duration) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var svg_text = create_item(cid, "text", {
            transform: "translate(" + x + ")",
            y: y,
            "font-size": "12px"
        });
        text_to_tspans(cid, svg_text, value + "x" + duration);
        frag.appendChild(svg_text);
        return frag;
    });
}

function gui_envgen_erase_text(cid, tag) {
    gui(cid).get_gobj(tag)
    .q("text", function(svg_text) {
        svg_text.parentNode.removeChild(svg_text);
    });
}

function gui_envgen_move_xlet(cid, tag, type, i, x, y, basex, basey) {
    gui(cid).get_elem(tag + type + i, {
        x: x - basex,
        y: y - basey
    });
}

exports.add_popup = add_popup;

// Kludge to get popup coords to fit the browser's zoom level. As of v0.16.1
// it appears nw.js fixed the bug that required this kludge. The only versions
// affected then are
// a) Windows, which is pinned to version 0.14.7 to support XP and
// b) OSX 10.8 which requires 0.14.7 to run.
// So we do a version check for "0.14.7" to see whether the kludge is
// needed.
function zoom_kludge(zoom_level) {
    var zfactor;
    switch(zoom_level) {
        case -7: zfactor = 0.279; break;
        case -6: zfactor = 0.335; break;
        case -5: zfactor = 0.402; break;
        case -4: zfactor = 0.483; break;
        case -3: zfactor = 0.58; break;
        case -2: zfactor = 0.695; break;
        case -1: zfactor = 0.834; break;
        case 1: zfactor = 1.2; break;
        case 2: zfactor = 1.44; break;
        case 3: zfactor = 1.73; break;
        case 4: zfactor = 2.073; break;
        case 5: zfactor = 2.485; break;
        case 6: zfactor = 2.98; break;
        case 7: zfactor = 3.6; break;
        case 8: zfactor = 4.32; break;
        default: zfactor = 1;
    }
    return zfactor;
}

function gui_canvas_popup(cid, xpos, ypos, canprop, canopen, cansaveas, isobject) {
    // Get page coords for top of window, in case we're scrolled
    gui(cid).get_nw_window(function(nw_win) {
        // ico@vt.edu updated win_left and win_top for the 0.46.2
        var win_left = nw_win.window.scrollX,
            win_top = nw_win.window.scrollY,
            zoom_level = nw_win.zoomLevel, // these were used to work
            zfactor,                       // around an old nw.js popup pos
                                           // bug. Now it's only necessary
                                           // on Windows, which uses v.0.14
            svg_view_box = nw_win.window.document.getElementById("patchsvg")
                .getAttribute("viewBox").split(" "); // need top-left svg origin

        // Check nw.js version-- if it's lts then we need the zoom_kludge...
        zfactor = process.versions.nw === "0.14.7" ? zoom_kludge(zoom_level) : 1;
        // Set the global popup x/y so they can be retrieved by the relevant
        // document's event handler
        popup_coords[0] = xpos;
        popup_coords[1] = ypos;
        //popup_coords[0] = xpos;
        //popup_coords[1] = ypos;
        popup_menu[cid].items[0].enabled = canprop;
        popup_menu[cid].items[1].enabled = canopen;
        popup_menu[cid].items[2].enabled = cansaveas;

        // We'll use "isobject" to enable/disable "To Front" and "To Back"
        //isobject;

        // We need to round win_left and win_top because the popup menu
        // interface expects an int. Otherwise the popup position gets wonky
        // when you zoom and scroll...
        xpos = Math.floor(xpos * zfactor) - Math.floor(win_left * zfactor);
        ypos = Math.floor(ypos * zfactor) - Math.floor(win_top * zfactor);

        // Now subtract the x and y offset for the top left corner of the svg.
        // We need to do this because a Pd canvas can have objects with negative
        // coordinates. Thus the SVG viewbox will have negative values for the
        // top left corner, and those must be subtracted from xpos/ypos to get
        // the proper window coordinates.
        xpos -= Math.floor(svg_view_box[0] * zfactor);
        ypos -= Math.floor(svg_view_box[1] * zfactor);

        popup_coords[2] = xpos + nw_win.x;
        popup_coords[3] = ypos + nw_win.y;

        popup_menu[cid].popup(xpos, ypos);
    });
}

function popup_action(cid, index) {
    pdsend(cid, "done-popup", index, popup_coords[0], popup_coords[1]);
}

exports.popup_action = popup_action;

// Graphs and Arrays

// Doesn't look like we need this

//function gui_graph_drawborder(cid, tag, x1, y1, x2, y2) {
//    var g = get_gobj(cid, tag);
//    var b = create_item(cid, "rect", {
//        width: x2 - x1,
//        height: y2 - y1,
//        stroke: "black",
//        fill: "none",
//        id: tag
//    });
//    g.appendChild(b);
//}

// This sets a GOP subpatch or graph to be "greyed out" when the user
// has opened it to inspect its contents.  (I.e., it has its own window.)
// We never actually remove this tag-- instead we just assume that the
// GOP will get erased and redrawn when its time to show the contents
// again.
function gui_graph_fill_border(cid, tag) {
    gui(cid).get_gobj(tag, function(e) {
        e.classList.add("has_window");
    });
}

function gui_graph_deleteborder(cid, tag) {
    gui(cid).get_elem(tag, function(e) {
        e.parentNode.removeChild(b);
    });
}

function gui_graph_label(cid, tag, font_size, font_height, is_selected,
    legacy_mode, is_toplevel, array_of_attr_arrays) {
    // first let's check if we have any colors other than black. If so we
    // we will create a little rectangle next to the label to show the color.
    var show_color_rect = false;
    array_of_attr_arrays.forEach(function(e) {
        var c;
        if (!show_color_rect) {
            c = attr_array_to_object(e).color;
            show_color_rect = (c !== "black" && (c !== "#000000" && c !== "#00000000"));
        }
    });

    // if the graph only holds a single array, don't display the color
    if (array_of_attr_arrays.length <= 1) {
        show_color_rect = false;
    }

    array_of_attr_arrays.forEach(function(e, i) {
        var a = attr_array_to_object(e),
            narrays = array_of_attr_arrays.length;
        // a.label for the label
        // a.color for the color
        gui(cid).get_elem("patchsvg", function(elem) {
            var x, y;
            if (!!legacy_mode) { // Pd Vanilla labels go above the box
                y = -font_height * (narrays - (i + 1)) - 1;
            } else { // In L2Ork they go inside the box
                // shift the label to the right if we're displaying a small
                // rectangle to show the color
                x = show_color_rect ? 17 : 2;
                y = font_height * (i + 1);
            }
            gui_text_new(cid, tag, "graph_label", !!is_selected,
                x, y, a.label, font_size, is_toplevel);
        })
        .get_gobj(tag)
        .append(function(frag) {
            var colorbar;
            if (legacy_mode == 0 && show_color_rect) {
                colorbar = create_item(cid, "rect", {
                    fill: a.color,
                    stroke: "black",
                    "stroke-width": 1,
                    x: 4,
                    y: font_height * i + (font_height * 0.3),
                    width: 10,
                    height: 10
                });
                frag.appendChild(colorbar);
            }
            return frag;
        })
        .get_elem(tag + "text", function(e) {
            e.id = tag + "text" + i;
        });
    });
}

function gui_graph_vtick(cid, tag, x, up_y, down_y, tick_pix, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var up_tick,
            down_tick;
        // Don't think these need an ID...
        up_tick = create_item(cid, "line", {
            stroke: "black",
            x1: x - basex,
            y1: up_y - basey,
            x2: x - basex,
            y2: up_y - tick_pix - basey
        });
        down_tick = create_item(cid, "line", {
            stroke: "black",
            x1: x - basex,
            y1: down_y - basey,
            x2: x - basex,
            y2: down_y + tick_pix - basey
        });
        frag.appendChild(up_tick);
        frag.appendChild(down_tick);
        return frag;
    });
}

function gui_graph_htick(cid, tag, y, r_x, l_x, tick_pix, basex, basey) {
    gui(cid).get_gobj(tag)
    .append(function(frag) {
        var left_tick,
            right_tick;
        // Don't think these need an ID...
        left_tick = create_item(cid, "line", {
            stroke: "black",
            x1: l_x - basex,
            y1: y - basey,
            x2: l_x - tick_pix - basex,
            y2: y - basey,
            id: "tick" + y
        });
        right_tick = create_item(cid, "line", {
            stroke: "black",
            x1: r_x - basex,
            y1: y - basey,
            x2: r_x + tick_pix - basex,
            y2: y - basey
        });
        frag.appendChild(left_tick);
        frag.appendChild(right_tick);
        return frag;
    });
}

function gui_graph_tick_label(cid, tag, x, y, text, font, font_size, font_weight, basex, basey, tk_label_anchor) {
    gui(cid).get_gobj(tag)
    .append(function(frag, w) {
        var svg_text, text_node, text_anchor, alignment_baseline;
        // We use anchor identifiers from the tk toolkit:
        //
        // "n" for north, or aligned at the top of the text
        // "s" for south, or default baseline alignment
        // "e" for east, or text-anchor at the end of the text
        // "w" for west, or default text-anchor for left-to-right languages
        //
        // For x labels the tk_label_anchor will either be "n" for labels at the
        // bottom of the graph, or "s" for labels at the top of the graph
        //
        // For y labels the tk_label_anchor will either be "e" for labels at the
        // right of the graph, or "w" for labels at the right.
        //
        // In each case we want the label to be centered around the tick mark.
        // So we default to value "middle" if we didn't get a value for that
        // axis.
        text_anchor = tk_label_anchor === "e" ? "end" :
            tk_label_anchor === "w" ? "start" : "middle";
        alignment_baseline = tk_label_anchor === "n" ? "hanging" :
            tk_label_anchor === "s" ? "auto" : "middle";
        svg_text = create_item(cid, "text", {
            // need a label "y" relative to baseline
            x: x - basex,
            y: y - basey,
            "text-anchor": text_anchor,
            "alignment-baseline": alignment_baseline,
            "font-size": pd_fontsize_to_gui_fontsize(font_size) + "px",
        });
        text_node = w.document.createTextNode(text);
        svg_text.appendChild(text_node);
        frag.appendChild(svg_text);
        return frag;
    });
}

function gui_canvas_drawredrect(cid, x1, y1, x2, y2) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        var g = gui_gobj_new(cid, cid, cid, cid, "gop_rect", x1, y1, 1, 0, "rect", "rect");
        var r = create_item(cid, "rect", {
            width: x2 - x1,
            height: y2 - y1,
            id: "gop_rect"
        });
        g.appendChild(r);
        svg_elem.appendChild(g);
    });
}

function gui_canvas_deleteredrect(cid) {
    // We need to check for existence here, because the first
    // time setting GOP in properties, there is no red rect yet.
    // But setting properties when the subpatch's window is
    // visible calls glist_redraw, and glist_redraw will try to delete
    // the red rect _before_ it's been drawn in this case.
    // Unfortunately, it's quite difficult to refactor those c
    // functions without knowing the side effects.  But ineffectual
    // gui calls should really be minimized-- otherwise it's simply
    // too difficult to debug what's being passed over the socket.
    gui(cid).get_gobj(cid, function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_canvas_redrect_coords(cid, x1, y1, x2, y2) {
    gui(cid).get_gobj(cid, function(e) {
        elem_move(e, x1, y1);
    })
    .get_elem("gop_rect", {
        width: x2 - x1,
        height: y2 - y1
    });
}

//  Cord Inspector (a.k.a. Magic Glass)

// For clarity, this probably shouldn't be a gobj.  Also, it might be easier to
// make it a div that lives on top of the patchsvg
function gui_cord_inspector_new(cid, font_size) {
    var g = get_gobj(cid, "cord_inspector"),
        ci_rect = create_item(cid, "rect", { id: "cord_inspector_rect" }),
        ci_poly = create_item(cid, "polygon", { id: "cord_inspector_polygon" }),
        ci_text = create_item(cid, "text", {
            id: "cord_inspector_text",
            "font-size": pd_fontsize_to_gui_fontsize(font_size) + "px",
        }),
        text_node = patchwin[cid].window.document.createTextNode("");
    ci_text.appendChild(text_node);
    g.appendChild(ci_rect);
    g.appendChild(ci_poly);
    g.appendChild(ci_text);
}

function gui_cord_inspector_update(cid, text, basex, basey, bg_size, y1, y2, moved) {
    var gobj = get_gobj(cid, "cord_inspector"),
        rect = get_item(cid, "cord_inspector_rect"),
        poly = get_item(cid, "cord_inspector_polygon"),
        svg_text = get_item(cid, "cord_inspector_text"),
        polypoints_array;
    gobj.setAttributeNS(null, "transform",
            "translate(" + (basex + 8.5) + "," + (basey + 0.5) + ")");
    gobj.setAttributeNS(null, "pointer-events", "none");
    gobj.classList.remove("flash");
    // Lots of fudge factors here, tailored to the current default font size
    configure_item(rect, {
        x: 5,
        y: y1 - basey,
        width: bg_size - basex,
        height: y2 - basey + 10
    });
    polypoints_array = [0,0,8,6.5,8,-6.5];
    configure_item(poly, {
        points: polypoints_array.join()
    });
    configure_item(svg_text, {
        x: 10,
        y: 3.5,
    });
    // set the text
    svg_text.textContent = text;
}

function gui_cord_inspector_erase(cid) {
    gui(cid).get_gobj("cord_inspector", function(e) {
        e.parentNode.removeChild(e);
    });
}

function gui_cord_inspector_flash(cid, state) {
    gui(cid).get_elem("cord_inspector_text", function(e) {
        if (state === 1) {
            e.classList.add("flash");
        } else {
            e.classList.remove("flash");
        }
    });
}

// Window functions

function gui_raise_window(cid) {
    // Check if the window exists, for edge cases like
    // [vis 1, vis1(---[send this_canvas]
    //post("gui_raise_window cid=" + cid);
    gui(cid).get_nw_window(function(nw_win) {
        nw_win.focus();
    });
}

// Unfortunately DOM window.focus doesn't actually focus the window, so we
// have to use the chrome API
function gui_raise_pd_window() {
    pd_window.window.focus();
    /*
    chrome.windows.getAll(function (w_array) {
        chrome.windows.update(w_array[0].id, { focused: true });
    });
    */
}

// ico@vt.edu 2022-11-09: reimplemented this function to make it
// work with newer nw.js (should also work with older ones)
function walk_window_list(cid, offset) {
    /*
    post("walk_window_list\n  patchwin=" + patchwin +
         "\n  patchwin[cid]=" + patchwin[cid] +
         "\n  cid=" + cid + " offset=" + offset +
         "\n  length=" + Object.keys(patchwin).length +
         "\n  value_at_index_0=" + Object.keys(patchwin)[0]);
    */
    var i, next, match = -1;
    var win_array_length = Object.keys(patchwin).length;

/*
    for (i = 0; i < win_array_length; i++) {
        if (Object.keys(patchwin)[i]) {
            //post("O=" + Object.keys(patchwin)[i] + " patchwin=" +
            //     patchwin[Object.keys(patchwin)[i]]);
            gui(Object.keys(patchwin)[i]).get_nw_window(function(nw_win) {
                post("window title:" + nw_win.title);
            }
        )};
    }
*/

    for (i = 0; i < win_array_length; i++) {
        if (Object.keys(patchwin)[i] === cid) {
            match = i;
            break;
        }
    }
    if (match !== -1) {
        next = (((match + offset) % win_array_length) // modulo...
                + win_array_length) % win_array_length; // handle negatives
        gui_raise_window(Object.keys(patchwin)[next]);
    } else if (cid === "pd_window" && Object.keys(patchwin).length > 0) {
        // for Windows and Linux, fall back here if we are passing
        // "pd_window" from the main window (see pd_m.win.nextwin in index.js)
        gui_raise_window(Object.keys(patchwin)
            [(offset === 1 ? 0 : Object.keys(patchwin).length - 1)])
    } else {
        post("error: cannot find last focused window.");
    }
}

function raise_next(cid) {
    walk_window_list(cid, 1);
}

exports.raise_next = raise_next;

function raise_prev(cid) {
    walk_window_list(cid, -1);
}

exports.raise_prev = raise_prev;

exports.raise_pd_window= gui_raise_pd_window;

// Openpanel and Savepanel

var file_dialog_object;

function file_dialog(cid, type, target, start_path) {
    var query_string = (type === "open" ?
                        "openpanelSpan" : "savepanelSpan"),
        input_span,
        input_elem,
        input_string,
        dialog_options,
        win;
    // We try opening the dialog in the last focused window. There's an
    // edge case where [loadbang]--[openpanel] will trigger before the
    // window has finished loading. In that case we just trigger the
    // dialog in the main Pd window.
    win = last_focused && patchwin[last_focused] ? patchwin[last_focused] :
        pd_window;
    input_span = win.window.document.querySelector("#" + query_string);
    // We have to use an absolute path here because of a bug in nw.js 0.14.7
    if (!path.isAbsolute(start_path)) {
        start_path = path.join(pwd, start_path);
    }
    // We also have to inject html into the dom because of a bug in nw.js
    // 0.14.7. For some reason we can't just change the value of nwworkingdir--
    // it just doesn't work. So this requires us to have the parent <span>
    // around the <input>. Then when we change the innerHTML of the span the
    // new value for nwworkingdir magically works.
    if(nw_os_is_windows) {
        start_path = start_path.replace(/\//g, '\\');
    }
    dialog_options = {
        style: "display: none;",
        type: "file",
        id: type === "open" ? "openpanel_dialog" : "savepanel_dialog",
        // using an absolute path here, see comment above
        nwworkingdir: start_path
    };
    if (type !== "open") {
        dialog_options.nwsaveas = "";
    }
    input_string = build_file_dialog_string(dialog_options);
    input_span.innerHTML = input_string;
    // Now that we've rebuilt the input element, let's get a reference to it...
    input_elem = win.window.document.querySelector("#" +
        (type === "open" ? "openpanel_dialog" : "savepanel_dialog"));
    // And add an event handler for the callback
    input_elem.onchange = function() {
        // reset value so that we can open the same file twice
        file_dialog_callback(target, this.value);
        this.value = null;
        console.log("openpanel/savepanel called");
    };
    win.window.setTimeout(function() {
        input_elem.click(); },
        300
    );
}

exports.file_dialog = file_dialog;

function file_dialog_obj(obj) {
    file_dialog_object = obj;
}

exports.file_dialog_obj = file_dialog_obj;

// ico@vt.edu 2021-10-31: we expose open and save panels
// for the coll and other cyclone objects

function gui_openpanel(cid, target, path, mode) {
    path = path || "./";
    if(nw_os_is_osx || nw_os_is_linux)
        if(!path.endsWith('/'))
            path += '/';
    if(nw_os_is_linux)
        path += '*';
    //post("gui_openpanel "+cid+" "+mode+" "+target+" <"+path+">");
    pdsend(file_dialog_object, "trigger", mode, target, path);
}

exports.gui_openpanel = gui_openpanel;

function gui_savepanel(cid, target, path) {
    path = path || "./";
    if(nw_os_is_linux)
        if(!path.endsWith('/'))
            path += '/';
    pdsend(file_dialog_object, "trigger", 3, target, path);
}

exports.gui_savepanel = gui_savepanel;

function file_dialog_callback(target, files) {
    pdsend(target, "callback", ...files.split('|').map(file => enquote(defunkify_windows_path(file))));
}

exports.file_dialog_callback = file_dialog_callback;

// Used to convert the ["key", "value"...] arrays coming from
// Pd to a javascript object. This is a hack that I employ because
// I had already implemented JSON arrays in the Pd->GUI interface
// and didn't feel like adding object notation.
function attr_array_to_object(attr_array) {
    var i,
        len = attr_array.length,
        obj = {};
    for (i = 0; i < len; i += 2) {
        obj[attr_array[i]] = attr_array[i+1];
    }
    return obj;
}

function gui_gatom_dialog(did, attr_array) {
    dialogwin[did] = create_window(did, "gatom", 259, 278-5,
        popup_coords[2], popup_coords[3],
        attr_array_to_object(attr_array));
}

function gui_gatom_activate(cid, tag, state) {
    //post("gui_gatom_activate tag=" + tag + " state=" + state);
    gui(cid).get_gobj(tag, function(e) {
        if (state !== 0) {
            e.classList.add("activated");
        } else {
            e.classList.remove("activated");
        }
    });
}

function gui_gatom_css(cid, tag, elem, prop, args) {
    var target = null;
    gui(cid).get_gobj(tag, function(item) {
        target = item.getElementsByClassName(elem);
        target[0].style.setProperty(prop, args.join(' '));
    });
}

function gui_dropdown_dialog(did, attr_array) {
    // Just reuse the "gatom" dialog (this is not true anymore, see below)
    // ico@vt.edu 2020-08-21: made this into a separate dialog due to inability to easily retitle
    // the window
    dialogwin[did] = create_window(did, "dropdown", 228, 268-5,
        popup_coords[2], popup_coords[3],
        attr_array_to_object(attr_array));
    // ico@vt.edu 2020-08-21: the following does not work because the window is not created yet?
    //dialogwin[did].window.document.getElementById("titlebar_title").innerHTML = "dropdown properties";
}

function dropdown_populate(w, label_array, current_index) {
    var ol = w.document.querySelector("#dropdown_list ol");
    // clear it out
    ol.innerHTML = '';
    label_array.forEach(function(text, i) {
        var li = w.document.createElement("li");
        li.textContent = text;
        li.setAttribute("data-index", i);
        if (i === current_index) {
            li.classList.add("highlighted");
        }
        ol.appendChild(li);
    });
}

function gui_dropdown_activate(cid, obj_tag, tag, current_index,
    font_size, state, label_array) {
    var g, select_elem, svg_view, g_bbox,
        doc_height,    // document height, excluding the scrollbar
        menu_height, // height of the list of elements inside the div
        div_y,       // position of div containing the dropdown menu
        div_max,     // max height of the div
        scroll_y,
        offset_anchor; // top or bottom

    // Annoying: obj_tag is just the "x"-prepended hex value for the object,
    // and tag is the one from rtext_gettag that is used as our gobj id
    gui(cid).get_elem("patchsvg", function(svg_elem, w) {
        g = get_gobj(cid, tag);
        if (state !== 0) {
            svg_view = svg_elem.viewBox.baseVal;
            select_elem = w.document.querySelector("#dropdown_list");
            dropdown_populate(w, label_array, current_index);
            // stick the obj_tag in a data field
            select_elem.setAttribute("data-callback", obj_tag);
            // display the menu so we can measure it

            g_bbox = g.getBoundingClientRect();
            // Measuring the document height is tricky-- the following
            // method is the only reliable one I've found. And even here,
            // if you display the select_elem as inline _before_ measuring
            // the doc height, the result ends up being _smaller_. No idea.
            doc_height = w.document.documentElement.clientHeight;
            // Now let's display the select_elem div so we can measure it
            select_elem.style.setProperty("display", "inline");
            menu_height = select_elem.querySelector("ol")
                .getBoundingClientRect().height;
            scroll_y = w.scrollY;
            // If the area below the object is smaller than 75px, then
            // display the menu above the object.
            // If the entire menu won't fit below the object but _will_
            // fit above it, display it above the object.
            // If the menu needs a scrollbar, display it below the object
            if (doc_height - g_bbox.bottom <= 75
                || (menu_height > doc_height - g_bbox.bottom
                    && menu_height <= g_bbox.top)) {
                // menu on top
                offset_anchor = "bottom";
                div_max = g_bbox.top - 2;
                div_y = doc_height - (g_bbox.top + scroll_y);
            }
            else {
                // menu on bottom (possibly with scrollbar)
                offset_anchor = "top";
                div_max = doc_height - g_bbox.bottom - 2;
                div_y = g_bbox.bottom + scroll_y;
            }
            // set a max-height to force scrollbar if needed
            select_elem.style.setProperty("max-height", div_max + "px");
            select_elem.style.setProperty("left",
                (g_bbox.left + w.scrollX) + "px");
            // Remove "top" and "bottom" props to keep state clean
            select_elem.style.removeProperty("top");
            select_elem.style.removeProperty("bottom");
            // Now position the div relative to either the "top" or "bottom"
            select_elem.style.setProperty(offset_anchor, div_y + "px");
            select_elem.style.setProperty("font-size",
                pd_fontsize_to_gui_fontsize(font_size) + "px");
            select_elem.style.setProperty("min-width", g.getBBox().width + "px");
            w.canvas_events.dropdown_menu();
        } else {
            //post("deactivating dropdown menu");
            // Probably want to send this
            pdsend(cid, "key 0 Control 0 1 0");
        }
    });
}

function gui_iemgui_dialog(did, attr_array) {
    //for (var i = 0; i < attr_array.length; i++) {
    //    attr_array[i] = '"' + attr_array[i] + '"';
    //}
    // ico@vt.edu: updated window size to match actual, thereby minimizing the flicker
    // We are subtracting 25 for the menu
    // ico@vt.edu: since adding frameless window, we use top 20px for draggable titlebar,
    // so now we subtract only 5 (25-20)

    //post("gui_iemgui_dialog " + attr_array);

    // we also need adjustment for flatgui/knob (in the menu, a.k.a. footils), and moonlinb/msknob
    create_window(did, "iemgui", 305, 366,
        popup_coords[2] + 10, popup_coords[3] + 60,
        attr_array_to_object(attr_array));
}

function gui_image_dialog(did, attr_array) {
    create_window(did, "image", 303, 651,
        popup_coords[2] + 10, popup_coords[3] + 60,
        attr_array_to_object(attr_array));
}

function gui_dialog_set_field(did, field_name, value) {
    var elem;
    //post("gui_dialog_set_field " + did + " " + field_name + " " + value);
    // ico@vt.edu 2021-10-21: hack-ish solution to an error that
    // breaks the gui: try to gracefully handle inability to locate
    // a dialog window.
    try {
        var elem = dialogwin[did].window.document.getElementsByName(field_name)[0];
    } catch(error) {
        // this usually happens when a dialog is still being created but the backend
        // already thinks it is ready because it has its own gfxstub created, so
        // we need to gracefully exit here
        //post("error: iemgui dialog window id:" + did + " " + field_name +
        //     ":" + value + " update failed... dialog window not found (" + error + ")");
    }
    // ico@vt.edu 2021-10-21: here we not only check that the elem is not null
    // but also whether the window has fully loaded.
    if (elem && dialogwin[did].window.document.readyState === "complete")
    {
        //post ("dialog document state is " + dialogwin[did].window.document.readyState);
        // since g_all_guis.c iemgui_update_properties blindly calls all the possible
        // options that not all objects have, here we check if the requested option is valid
        if (elem.type === "checkbox") {
            elem.checked = (value === 1 ? true : false);
        } else if (elem.type === "select-one") {
            elem.selectedIndex = value;
        } else if (elem.type === "color") {
            var hex_string = Number(value).toString(16);
            if (hex_string.length === 8) {
                //post("gui_dialog_set_field starting color: " + hex_string);
                hex_string = hex_string.slice(-2) + hex_string.slice(0, 6);
                //post("gui_dialog_set_field ending color: " + hex_string);
            }
            while(hex_string.length < 6) {
                hex_string = "0" + hex_string;
            }
            if (hex_string.length === 6) {
                hex_string = hex_string + "ff";
            }
            var color_string = "#" +
                (hex_string === "0" ? "000000ff" : hex_string);
            elem.value = color_string;
            //post("gui_dialog_set_field color " + color_string);
        } else {
            elem.value = value;
        }
        if (elem.type !== "hidden")
            elem.onchange();
    }
    //dialogwin[did].window.update_attr(elem);
}

// Used when undoing a font size change when the font dialog is open
function gui_font_dialog_change_size(did, font_size) {
    var button;
    if (dialogwin[did]) {
        button = dialogwin[did].window.document.getElementById(font_size);
        button.click();
    } else {
        post("error: no font dialogwin!");
    }
}

function gui_menu_font_change_size(canvas, newsize) {
    pdsend(canvas, "menufont", newsize);
    	// ico@vt.edu 2020-08-24: changed to use submenu
    	// this was the following
        /*+document.querySelector('input[name="font_size"]:checked').value,
        current_size,
        100,
        0 // "$noundo" from pd.tk-- not sure what it does*/
}

exports.gui_menu_font_change_size = gui_menu_font_change_size;

function gui_menu_font_set_initial_size(cid, size) {
	//post("gui_menu_font_set_initial_size " + cid + " " + size);
	gui(cid).get_nw_window(function(nw_win) {
		if (cid !== "nobody") {
            if (nw_os_is_osx) {
                nw_win.window.set_menu_modals(cid, true);
                //nw_win.window.focus();
            }
    		nw_win.window.init_menu_font_size(size);
    		//post("this should work");
    	}
    });
}

function gui_array_new(did, count) {
    //post("gui_array_new last_focused=" + last_focused);
    var attr_array = [{
        array_gfxstub: did,
        array_name: "array" + count,
        array_size: 100,
        array_flags: 3,
        array_fill: get_css_svg_color(last_focused, 2),
        array_outline: get_css_svg_color(last_focused, 3),
        array_in_existing_graph: 0
    }];
    dialogwin[did] = create_window(did, "canvas",
        280, 243, patchwin[last_focused].x + patchwin[last_focused].width/2 - 140,
        patchwin[last_focused].y + 10,
        attr_array);
}

function gui_canvas_dialog(did, attr_arrays) {
	//post("gui_canvas_dialog");
    var i, j, inner_array, prop;
    // Convert array of arrays to an array of objects
    for (i = 0; i < attr_arrays.length; i++) {
        attr_arrays[i] = attr_array_to_object(attr_arrays[i]);
        for (prop in attr_arrays[i]) {
            if (attr_arrays[i].hasOwnProperty(prop)) {
                console.log("array: prop is " + prop);
            }
        }
    }
    var has_array = (attr_arrays.length > 1 ? 1 : 0);
    /*
    post("array.length=" + attr_arrays.length + " has_array=" + has_array +" width=" +
    	(230 - (8 * has_array)) + " height=" +
    	(attr_arrays.length > 1 ? 494-25+(attr_arrays.length > 2 ? 38 : 0) : 392-25));
    */
    dialogwin[did] = create_window(did, "canvas",
        // ico@vt.edu: property dialog size is larger when one has
        // arrays inside the canvas.
        // 1 for regular canvas and 2 for a canvas with 1 array,
        // 3 for canvas with 2 arrays, etc.
        // We also substract here 5 for the smaller top bar...
        268 + (8 * has_array),
        (attr_arrays.length > 1 ? 563-25 : 420-25),
        popup_coords[2], popup_coords[3],
        attr_arrays);
}

function gui_data_dialog(did, data_string) {
    dialogwin[did] = create_window(did, "data", 205, 323 + (22 * nw_os_is_osx),
        popup_coords[2], popup_coords[3],
        data_string);
}

function gui_text_dialog_clear(did) {
    //post("gui_text_dialog_clear <"+text_dialog_text[did]+">");
    if (dialogwin[did]) {
        dialogwin[did].window.textarea_clear();
    }
}

function gui_text_dialog_append(did, line) {
    //post("gui_text_dialog_append <"+line+">");
    if (dialogwin[did]) {
        //post("...yes");
        dialogwin[did].window.textarea_append(line);
    }
}

function gui_text_dialog_set_dirty(did, state) {
    if (dialogwin[did]) {
        dialogwin[did].window.set_dirty(state !== 0);
    }
}

function gui_text_dialog(did, name, x, y, width, height, font_size) {
    //post("gui_text_dialog "+did+" "+name+" "+width+" "+height+" "+font_size);
    dialogwin[did] = create_window(did, "text", width, height, x, y,
        {
            fontsize: font_size,
            title: name
        }
    );
}

function dialog_raise(did) {
    dialogwin[did].focus();
}

function gui_text_dialog_raise(did) {
    if (dialogwin[did]) {
        dialog_raise(did);
    }
}

function gui_text_dialog_close_from_pd(did, signoff) {
    if (dialogwin[did]) {
        dialogwin[did].window.close_from_pd(signoff !== 0);
    }
}

function gui_remove_gfxstub(did) {
    if (dialogwin[did] !== undefined && dialogwin[did] !== null) {
        dialogwin[did].close(true);
        dialogwin[did] = null;
    }
}

function gui_font_dialog(cid, font_size) {
    //var attrs = { canvas: cid, font_size: font_size };
    //dialogwin[gfxstub] = create_window(gfxstub, "font", 136, 187, 0, 0,
    //    attrs);
    // ico@vt.edu: 2020-08-24: we don't need this anymore since everything
    // is now inside the menu
}

function gui_external_dialog(did, external_name, attr_array) {
    create_window(did, "external", 202, 323 + (22 * nw_os_is_osx),
        popup_coords[2], popup_coords[3],
        {
            name: external_name,
            attributes: attr_array
        });
}

function gui_abstractions_dialog(cid, gfxstub, filebased_abs, private_abs) {
    var attrs = { canvas: cid, filebased_abs: filebased_abs,
                    private_abs: private_abs };
    dialogwin[gfxstub] = create_window(gfxstub, "abstractions", 300, 
        Math.min(600, (private_abs.length*10 + 190+(nw_os_is_osx?20:0))), 0, 0, attrs);
}

// Global settings

function gui_pd_dsp(state) {
    if (pd_window !== undefined) {
        pd_window.document.getElementById("dsp_control").checked = !!state;
    }
}

// dialog_prefs.html
function open_prefs() {
    if (!dialogwin["prefs"]) {
        create_window("prefs", "prefs", 503,
            552 + (nw_os_is_osx * 16), 0, 0, null);
    } else {
        dialog_raise("prefs");
    }
}

exports.open_prefs = open_prefs;

// dialog_search.html
function open_search() {
    if (!dialogwin["search"]) {
        create_window("search", "search", 350, 400, 20, 20, null);
    } else {
        dialog_raise("search");
    }
}

exports.open_search = open_search;

// This is the same for all windows (initialization is in pd_menus.js).
var recent_files_submenu = null;
var recent_files = null;

// same for open windows shared submenu
var open_windows_submenu = null

// We need to jump through some hoops here since JS closures capture variables
// by reference, which causes trouble when closures are created within a
// loop.
function recent_files_callback(i) {
    return function() {
        var fname = recent_files[i];
        //post("clicked recent file: "+fname);
        open_file(fname);
    }
}

function populate_recent_files(submenu) {
    if (submenu) recent_files_submenu = submenu;
    if (recent_files && recent_files_submenu) {
        //post("recent files: " + recent_files.join(" "));
        while (recent_files_submenu.items.length > 0)
            recent_files_submenu.removeAt(0);
        for (var i = 0; i < recent_files.length; i++) {
            var item = new nw.MenuItem({
                label: path.basename(recent_files[i]),
                tooltip: recent_files[i]
            });
            item.click = recent_files_callback(i);
            recent_files_submenu.append(item);
        }
        if (recent_files_submenu.items.length > 0) {
            recent_files_submenu.append(new nw.MenuItem({
                type: "separator"
            }));
            var item = new nw.MenuItem({
                label: lang.get_local_string("menu.clear_recent_files"),
                tooltip: lang.get_local_string("menu.clear_recent_files_tt")
            });
            item.click = function() {
                pdsend("pd clear-recent-files");
            };
            recent_files_submenu.append(item);
        }
    }
}

exports.populate_recent_files = populate_recent_files;

// We need to jump through some hoops here since JS closures capture variables
// by reference, which causes trouble when closures are created within a
// loop.
function open_window_callback(i) {
    return function() {
        var cid = Object.keys(patchwin)[i];
        //post("clicked open window callback: "+ cid);
        gui_raise_window(cid);
    }
}

function populate_open_windows(submenu) {
    //post("populate_open_windows");
    if (submenu) {
        open_windows_submenu = submenu;
        // if we have an argument that means this is only
        // for the purpose of initializing the pdgui's global var
        return;
    }
    else {
        setTimeout(function() {

            var i, cid;
            var titles = [];

            for (i = 0; i < Object.keys(patchwin).length; i++) {
                if (Object.keys(patchwin)[i]) {
                    gui(Object.keys(patchwin)[i]).get_nw_window(function(nw_win) {
                        //titles[i] = nw_win.title; // this one includes path
                        titles[i] = nw_win.title.split("  - ")[0]; // pathless
                        //post("window title:" + nw_win.title);
                    }
                )};
            }

            while (open_windows_submenu.items.length > 0)
                open_windows_submenu.removeAt(0);

            for (i = 0; i < titles.length; i++) {
                if (titles[i] != null) {
                    //pdgui.post("...adding " + title_array[i]);
                    var item = new nw.MenuItem({
                        label: titles[i],
                        tooltip: titles[i]
                    });
                    item.click = open_window_callback(i);
                    open_windows_submenu.append(item);
                }
            }
        }, 250);
    }
}

exports.populate_open_windows = populate_open_windows;

function gui_recent_files(dummy, recent_files_array) {
    recent_files = recent_files_array;
    populate_recent_files(recent_files_submenu);
}

function gui_audio_properties(gfxstub, sys_indevs, sys_outdevs,
    pd_indevs, pd_inchans, pd_outdevs, pd_outchans, audio_attrs) {
    var attrs = audio_attrs.concat([
        "audio-apis", pd_apilist,
        "sys-indevs", sys_indevs,
        "sys-outdevs", sys_outdevs,
        "pd-indevs", pd_indevs,
        "pd-inchans", pd_inchans,
        "pd-outdevs", pd_outdevs,
        "pd-outchans", pd_outchans
        ]);
    //for (var i = 0; i < arguments.length; i++) {
    //    post("arg " + i + " is " + arguments[i]);
    //}
    if (dialogwin["prefs"] !== null) {
        dialogwin["prefs"].eval(null,
            "audio_prefs_callback("  +
            JSON.stringify(attrs) + ");"
        );
    }
}

function gui_midi_properties(gfxstub, sys_indevs, sys_outdevs,
    pd_indevs, pd_outdevs, midi_attrs) {
    var attrs = midi_attrs.concat([
        "midi-apis", pd_midiapilist,
        "midi-indev-names", sys_indevs,
        "midi-outdev-names", sys_outdevs,
        "pd-indevs", pd_indevs,
        "pd-outdevs", pd_outdevs,
        ]);
    //post("got back some midi props...");
    //for (var i = 0; i < arguments.length; i++) {
    //    post("arg " + i + " is " + arguments[i]);
    //}
    if (dialogwin["prefs"] !== null) {
        dialogwin["prefs"].eval(null,
            "midi_prefs_callback("  +
            JSON.stringify(attrs) + ");"
        );
    }
}

function gui_gui_properties(dummy, name, show_grid, grid_size, save_zoom,
                            autocomplete, autocomplete_prefix, autocomplete_relevance,
                            browser_doc, browser_path, browser_init,
                            autopatch_yoffset, curved_cords) {
    if (dialogwin["prefs"] !== null) {
        dialogwin["prefs"].window.gui_prefs_callback(name, show_grid, grid_size,
            save_zoom, autocomplete, autocomplete_prefix,  autocomplete_relevance,
            browser_doc, browser_path, browser_init, autopatch_yoffset, curved_cords);
    }
}

function gui_path_properties(dummy, use_stdpath, verbose, path_array) {
    if (dialogwin["prefs"] !== null) {
        dialogwin["prefs"].window.path_prefs_callback(use_stdpath, verbose, path_array);
    }
}

function gui_lib_properties(dummy, defeat_rt, flag_string, lib_array) {
    if (dialogwin["prefs"] !== null) {
        dialogwin["prefs"].window.lib_prefs_callback(defeat_rt, flag_string, lib_array);
    }
}

// Let's try a closure for gui skins
var skin = exports.skin = (function () {
    var dir = "css/";
    var preset = "default";
    var id;
    function set_css(win) {
        win.document.getElementById("page_style")
            .setAttribute("href", dir + preset + ".css");
    }
    return {
        get: function () {
            post("getting preset: " + dir + preset + ".css");
            return dir + preset + ".css";
        },
        set: function (name) {
            // ag: if the preset doesn't exist (e.g., user preset that
            // has disappeared), just stick to the default
            var base = process.platform === "darwin" ? (lib_dir + "/") : "";
            if (fs.existsSync(base + dir + name + ".css")) {
                preset = name;
            }
            for (id in patchwin) {
                if (patchwin.hasOwnProperty(id) && patchwin[id]) {
                    set_css(patchwin[id].window);
                }
            }
            // hack for the console
            pd_window.document.getElementById("page_style")
                .setAttribute("href", dir + preset + ".css");
        },
        apply: function (win) {
            set_css(win);
        }
    };
}());

function select_text(cid, elem, sel_start, sel_end) {
    var range, win = patchwin[cid].window;
    if (win.document.selection) {
        range = win.document.body.createTextRange();
        range.moveToElementText(elem);
        var len = elem.textContent.length,
            ms = Math.max(Math.min(sel_start, len), 0),
            me = Math.max(Math.min(sel_end, len), ms);
        if(sel_start != -1) range.moveStart("character", ms);
        if(sel_end != -1) range.moveEnd("character", me-len);
        range.select();
    } else if (win.getSelection) {
        range = win.document.createRange();
        range.selectNodeContents(elem);
        var len = elem.textContent.length,
            ms = Math.max(Math.min(sel_start, len), 0),
            me = Math.max(Math.min(sel_end, len), ms);
        if(sel_start != -1) range.setStart(elem.firstChild, ms);
        if(sel_end != -1) range.setEnd(elem.firstChild, me);
        win.getSelection().removeAllRanges();
        win.getSelection().addRange(range);
    }
}

// CSS: Cleanly separate style from content.
// Me: Ahhhh!
// Arnold: Get down!
// Me: Wat?
// CSS: Impossible...
// Arnold: Style this. *kappakappakappa*
// Me: Hey, you can't just go around killing people!
// Arnold: It's not human. It's a W3C Standard.
// Me: But how did it get here?
// Arnold: It travelled from the past.
// Me: What does it want?
// Arnold: It won't stop until your energy is completely eliminated.
// Me: What now?
// Arnold: Use this to find what you need. Then get the heck out of there!
function get_style_by_selector(w, selector) {
    var sheet_list = w.document.styleSheets,
        rule_list, i, j,
        len = sheet_list.length;
    for (i = 0; i < len; i++) {
        rule_list = sheet_list[i].cssRules;
        for (j = 0; j < rule_list.length; j++) {
            if (rule_list[j].type == w.CSSRule.STYLE_RULE &&
                rule_list[j].selectorText == selector) {
                return rule_list[j].style;
            }
        }
    }
    return null;
}

// for debugging purposes
exports.get_style_by_selector = get_style_by_selector;

// 2020-10-06 ico@vt.edu: the following deals with nw.js' discrepancy between
// positioning svg text, versus html paragraph (editable) text
var textarea_font_height_array_kludge = [
// zoom levels      -7  -6  -5  -4  -3  -2  -1  0   1   2   3   4   5   6   7
/* font size 8  */ [ 40, 48, 70, 90,116,140,150,133,133,136,133,135,136,133,135],
/* font size 10 */ [ 50, 70, 90,116,132,133,133,133,134,133,134,133,132,131,130],
/* font size 12 */ [ 80, 90,100,134,148,140,140,140,144,140,140,140,140,140,140],
/* font size 16 */ [ 90,100,120,120,120,120,120,120,120,115,115,115,115,115,115],
/* font size 24 */ [128,128,128,128,128,128,128,128,128,128,128,126,126,126,125],
/* font size 36 */ [127,124,124,122,122,122,122,122,122,122,121,121,120,121,121]
];

var textarea_y_offset_array_kludge = [
// zoom levels      -7  -6  -5  -4  -3  -2  -1  0   1   2   3   4   5   6   7
/* font size 8  */ [1.5,1.5,1.5,1.5,1.5,0. ,-2.,1.5,1.5,1.0,1.0,1.0,0.2,0.5,0.5],
/* font size 10 */ [0.5,0.5,0.5,1.5,0.5,1.5,0. ,0.5,0.7,0.8,0.8,1.0,0.5,0.5,0.8],
/* font size 12 */ [1.5,1.5,1.5,2.0,1.5,1.5,1.5,1.5,1.5,1.5,1.5,2.0,1.5,1.5,1.5],
/* font size 16 */ [1.5,1.5,-1.,1.5,1.0,1.5,0.0,1.5,1.5,1.5,1.2,1.5,1.0,0.7,1.2],
/* font size 24 */ [1.5,2.5,2.5,3.0,1.5,2.5,2.5,1.5,2.5,2.5,1.5,2.0,1.5,1.5,2.2],
/* font size 36 */ [1.5,1.5,1.5,3.0,1.5,1.5,1.5,2.5,2.5,1.5,2.0,2.5,1.7,1.6,1.9]
];

// helper function to get the right index inside the aforesaid kludge arrays
// used by functions below
function textarea_font_size_to_index(font_size) {
    switch(font_size) {
        case  8: return 0;
        case 10: return 1;
        case 12: return 2;
        case 16: return 3;
        case 24: return 4;
        case 36: return 5;
    }
}

function textarea_line_height_kludge(font_size, zoom) {
	return textarea_font_height_array_kludge
        [textarea_font_size_to_index(font_size)][zoom+7]+"%";
}

function textarea_y_offset_kludge(font_size, zoom) {
    return textarea_y_offset_array_kludge
        [textarea_font_size_to_index(font_size)][zoom+7];
}

function textarea_x_offset_kludge(font_size, zoom) {
	if (font_size === 36) {
		return -2;
	} else {
		return -0.5;
	}
}

function textarea_msg_y_offset_kludge(zoom) {
    if (zoom == 0) {
        return -1;
    } else if (zoom > 0) {
        return -0.5;
    } else {
        //default
        return 0;
    }
}

function gui_textarea(cid, tag, type, x, y, width_spec, height_spec, text,
    font_size, font_width, font_height, is_gop, state, sel_start, sel_end,
    nlet_width) {
    var range, svg_view, p,
        gobj = get_gobj(cid, tag), zoom;
    //post("gui_textarea tag="+tag+" type="+type+" text=<"+
    //    text+"> state="+state+" gobj="+gobj);
    //post("gui_textarea width_spec="+width_spec+" nlet_width="+nlet_width);

    // replace \v for \n and \u00A0 for " ". This is only used by the comments,
    // so it only affects the comment object, while the rest should be unaffected
    if (type === "comment") {
        //post("...replacing");
        text = text.replace(/\v/g, "\n");
        // we get rid of multiple consecutive \n because binbuf inserts
        // at some point \n behind ; which results in a growing comment
        // this, however, currently prevents inserted empty lines inside comments
        // to display properly without adding at least one space
        //text = text.replace(/\n+/g, "\n");
    }

    gui(cid).get_nw_window(function(nw_win) {
        zoom = nw_win.zoomLevel;
    });
    if (state !== 0) {
        // Make sure we're in editmode
        canvas_set_editmode(cid, 1);
        // scroll to the object in case not all of it is visible
        gui_canvas_scroll_to_gobj(cid, tag, 1);
        // Hide the gobj while we edit.  However, we want the gobj to
        // contribute to the svg's bbox-- that way when the new_object_textentry
        // goes away we still have the same dimensions.  Otherwise the user
        // can get strange jumps in the viewport when instantiating an object
        // at the extremities of the patch.
        // To solve this, we use 'visibility' instead of 'display', since it
        // still uses the hidden item when calculating the bbox.
        // (We can probably solve this problem by throwing in yet another
        // gui_canvas_get_scroll, but this seems like the right way to go
        // anyway.)

        // Hide elements:
        // all text objects except for the message box hide everything,
        // while the message box has a new approach that retains the svg
        // shape below it. LATER: we may want to:
        //     1) extend this to support nlets (currently we hide them);
        //     2) extend this to adjust patch cords as things are being edited, and
        //     3) extend this to all text objects.
        if (type === "msg") {
        	// Message approach
	        var i, nlets = patchwin[cid].window.document
	        	.getElementById(tag+"gobj").querySelectorAll(".xlet_control");
	        for (i = 0; i < nlets.length; i++) {
	        	nlets[i].style.setProperty("visibility", "hidden");        	
	        }
	        gui(cid).get_gobj(tag).q(".box_text", { visibility: "hidden" });
	    } else {
	    	// Anything else but message
            // Here we check if gobj is valid as some externals (e.g. ggee/image)
            // do not handle hiding here because they have non-convetional elements
            if (gobj) {
		        configure_item(gobj, { visibility: "hidden" });
                // Explicitly also hide text since it sometimes remains visible
                // (e.g. when a multi-line comment is activated and user deletes
                // a line or more from it, sometimes the original text is still
                // visible)
                gui(cid).get_gobj(tag).q(".box_text", { visibility: "hidden" });
            }
	    }

        p = patchwin[cid].window.document.createElement("p");
        configure_item(p, {
            id: "new_object_textentry",
            cid: cid,
            tag: tag,
            font_width: font_width,
            font_height: font_height,
            "type" : type,
            "width_spec": width_spec,
            "nlet_width": nlet_width
        });

        // ico@vt.edu 2021-05-10: hide overflow, so that the selection
        // does not stick out of the object when the object's text is
        // multiline and therefore implicitly has a line break character
        p.style.setProperty("overflow", "hidden");

        svg_view = patchwin[cid].window.document.getElementById("patchsvg")
            .viewBox.baseVal;
        if (type === "comment")
            p.classList.add("obj");
        else
            p.classList.add(type);
        p.contentEditable = "true";

        if (is_gop == 0) {
            // do we need to assign here color from the css theme instead?
            p.style.setProperty("background-color", "#f6f8f8");
            // ico@vt.edu 2021-05-10: added a bit of right padding to make
            // the activated box better match the regular one
            p.style.setProperty("padding-right", "2px");
        } else {
            // ico@vt.edu: added tweaks to ensure the GOP selection
            // border is near identical to that of its regular border
            p.style.setProperty("min-height", height_spec - 7 + "px");
            p.style.setProperty("padding-left", "2px");
            /* ico@vt.edu:
               should the graph be transparent or opaque? legacy compatibility
               suggests it should be transparent, although that may go against
               common sense UI design. Perhaps we can make this an option? If
               so, we could do so here. Better yet, we could capture background
               color of the subpatcher and use it to apply the same color here.
               p.style.setProperty("background-color", "white");
            */   
        }
        
        p.style.setProperty("left", (x - svg_view.x + textarea_x_offset_kludge(font_size, zoom)) + "px");
        p.style.setProperty("top", (y - svg_view.y + textarea_y_offset_kludge(font_size, zoom)) + "px");
        p.style.setProperty("font-size",
            pd_fontsize_to_gui_fontsize(font_size) + "px");
        p.style.setProperty("line-height",
        	textarea_line_height_kludge(font_size, zoom));
            //pd_fontsize_to_gui_fontsize(font_size) + 1 + "px");
        p.style.setProperty("transform", "translate(0px, " + 
            (zoom > 0 ? 0.5 : 0) + "px)");
        p.style.setProperty("max-width",
            width_spec > 0 ? width_spec + "ch" : "60ch");
        //p.style.setProperty("width", -width_spec - 2 + "px");
        p.style.setProperty("-webkit-padding-after", "1px");

        // ico@vt.edu 2021-10-08: disable shadowing effect on message boxes
        // since it obliterates visibility of its edges
        if (type == "msg") {
            p.style.setProperty("box-shadow", "none");
            p.style.setProperty("animation", "none");
        }

        // ico@vt.edu 2021-10-07: set the minimum width according to the width_spec
        // variable if it is less than zero.
        if (type !== "msg" && width_spec < 0) {
            p.style.setProperty("min-width", (-width_spec-3) + "px");
        }

        // ico@vt.edu 2021-03-29 (updated 2021-04-20 because activation of a comment
        // that spills over to the right results in a width truncation which only
        // applies to when the object is initially activated, thus requiring a slightly
        // different treatment of the two approaches):
        // here we use similar code as the one below in the oninput call, to ensure
        // that the initial object width is as it should be regardless of whether the
        // object is overlapping the right window edge or not. this only applies to
        // regular non-gop objects.
        //post("width_spec="+width_spec);
        if (is_gop == 0 && width_spec <= 0)
        {
            var tl = text.trim().length;
            var mw = parseInt(p.style.maxWidth, 10);
            //post("mw="+mw+" tl="+tl);
            if (tl <= 3 && width_spec == 0)
                p.style.setProperty("min-width", "3ch");
            else if (tl <= 3 && width_spec < 0 && nlet_width == 0)
                p.style.setProperty("min-width", "3ch");
            else if (tl > 3)
            {
                var text2lines = text.split('\n');
                var i, j, n;
                n = 0;
                for (i = 0; i < text2lines.length; i++) {
                    j = text2lines[i].length;
                    if (j > n) {
                        if (j < mw-1)
                            n = j;
                        else
                            n = mw;
                    }
                }
                if (n == 0)
                    n = mw;
                if (n < tl)
                    tl = n;
                // here we again ask for width_spec since this may have been
                // a multiline comment where still individual lines were no
                // longer than the 3 chars and the width_spec may have not been
                // set, so we need to fall back to the width of 3 even if we are
                // narrower unless our object cannot be smaller than minimum width
                // because of the number of inlets or outlets
                //post("tl="+tl+" width_spec="+width_spec);
                if (tl <= 3 && width_spec == 0)
                    p.style.setProperty("min-width", "3ch");
                else if (width_spec < 0 && nlet_width == 0)
                    p.style.setProperty("min-width", tl+"ch");
                else if (width_spec < 0 && font_width * tl > -width_spec)
                    p.style.setProperty("min-width", tl+"ch");
                else if (width_spec < 0)
                    p.style.setProperty("min-width",
                        (-width_spec-3)+"px");
            }
            //post("maxw="+mw+" minw="+tl+" n="+n);
        } else {
            // otherwise fall back to the old way
            p.style.setProperty("min-width",
                width_spec == 0 ? "3ch" :
                    (is_gop == 1 ?
                        (Math.abs(width_spec) - 2) + "px" :
                            (width_spec < 0 ? width_spec - 2 + "px" : width_spec+"ch")
            ));
        }
        if (is_gop !== 1 && width_spec <= 0)
        {
            // we are a new object box with no specified width, so we need to update
            // min-width when the text is less long than the maximum allowed 60px
            p.oninput = function(obj){
                // we use this to force min-width when entering new text into the object
                // and/or message boxes, so that we can ensure that objects don't get
                // shorter width than allowed by default. Once they have been given a
                // user-specified width or there is an \n, this is no longer an issue.
                var tl = p.innerText.length;
                var mw = parseInt(p.style.maxWidth, 10);
                if (tl <= 3 && width_spec == 0)
                    p.style.setProperty("min-width", "3ch");
                else if (tl <= 3 && width_spec < 0 && nlet_width == 0)
                    p.style.setProperty("min-width", "3ch");
                else if (tl > 3)
                {
                    var text2lines = p.innerText.split('\n');
                    var i, j, n;
                    n = 0;
                    for (i = 0; i < text2lines.length; i++) {
                        j = text2lines[i].length;
                        if (j > n) {
                            if (j < mw-1)
                                n = j;
                            else
                                n = mw;
                        }
                    }
                    if (n < tl)
                        tl = n;
                    // here we again ask for width_spec since this may have been
                    // a multiline comment where still individual lines were no
                    // longer than the 3 chars and the width_spec may have not been
                    // set, so we need to fall back to the width of 3 even if we are
                    // narrower
                    //post("tl="+tl+" width_spec="+width_spec);
                    if (tl <= 3 && width_spec == 0)
                        p.style.setProperty("min-width", "3ch");
                    else if (width_spec < 0 && nlet_width == 0)
                        p.style.setProperty("min-width", tl+"ch");
                    else if (width_spec < 0 && font_width * tl > -width_spec)
                        p.style.setProperty("min-width", tl+"ch");
                    //post("maxw="+mw+" minw="+tl+" n="+n);
                    else if (width_spec < 0)
                        p.style.setProperty("min-width",
                            (-width_spec-3)+"px");
                }
                var pheight = parseInt(p.offsetHeight / p.getAttribute('font_height'));
                //post("height="+ pheight + " " + p.getAttribute('font_height'));
                pdsend(cid, "cah", pheight);
                gui_message_update_textarea_border(cid, p, 1);

            }; 
        }
        //p.style.setProperty("white-space", "break-spaces");

        if (is_gop == 1) {
            p.style.setProperty("min-height", height_spec - 4 + "px");
        }
        // remove leading/trailing whitespace
        text = text.trim();
        p.textContent = text;
        // append to doc body
        //patchwin[cid].window.document.body.appendChild(p);
        // ico@vt.edu 2022-10-12: instead of putting it at the very top,
        // insert this below the dialogs and scrollbars
        var ref = patchwin[cid].window.document.getElementById("canvas_find");
        ref.parentNode.insertBefore(p, ref);
        if (type === "msg")
        {
            // ico@vt.edu 2020-09-30: New approach to drawing
            // messages that utilizes the original svg border
            p.style.setProperty("-webkit-padding-before", "2px");
            p.style.setProperty("-webkit-padding-after", "3px");
            p.style.setProperty("-webkit-padding-start", "0px");
            p.style.setProperty("-webkit-padding-end", "0px");
            p.style.setProperty("margin-left", "2.5px");
            p.style.setProperty("transform", "translate(0px, " +
                textarea_msg_y_offset_kludge(zoom) + "px)");
            p.style.setProperty("background-color", "");
            //post("line-height="+ parseInt(p.style.lineHeight) / 100 * font_size);
            //shove_svg_background_data_into_css(patchwin[cid].window,
            //    parseInt(get_gobj(cid, tag).getBoundingClientRect().height /
            //        (parseInt(p.style.lineHeight) / 100 * font_size)));
        	gui_message_update_textarea_border(cid, p, 1);
        }
        p.focus();
        select_text(cid, p, sel_start, sel_end);
        if (font_size === 36) {
            if (is_gop) {
                p.style.setProperty("padding", "2px 0px 2px 2.5px");
            } else {
                p.style.setProperty("padding", "2px 0px 2px 1.5px");
            }
        }
        if (state === 1) {
            patchwin[cid].window.canvas_events.text();
        } else {
            patchwin[cid].window.canvas_events.floating_text();
        }
    } else {
        configure_item(gobj, { visibility: "normal" });
        p = patchwin[cid].window.document.getElementById("new_object_textentry");
        if (p !== null) {
            p.parentNode.removeChild(p);
        }

        // MSG approach
        if (type === "msg") {
            var i, nlets = patchwin[cid].window.document
            	.getElementById(tag+"gobj").querySelectorAll(".xlet_control");
            for (i = 0; i < nlets.length; i++) {
            	nlets[i].style.setProperty("visibility", "visible");        	
            }
        }
        gui(cid).get_gobj(tag).q(".box_text", { visibility: "visible" });

        if (patchwin[cid].window.canvas_events.get_previous_state() ===
               "search") {
            patchwin[cid].window.canvas_events.search();
        } else {
            patchwin[cid].window.canvas_events.normal();
        }

        // GB: Autocomplete dropdown -- if the paragraph element of "new_object_textentry" is deleted,
        //     the dropdown of autocompletion shall be deleted also
        let autocomplete_dropdown = patchwin[cid].window.document.getElementById("autocomplete_dropdown");
        if (autocomplete_dropdown !== null) {
            autocomplete_dropdown.parentNode.removeChild(autocomplete_dropdown);
        }
    }
}

function gui_undo_menu(cid, undo_text, redo_text) {
    // we have to check if the window exists, because Pd starts
    // up with two unvis'd patch windows used for garrays. Plus
    // there may be some calls to subpatches after updating a dialog
    // (like turning on GOP) which call this for a canvas that has
    // been destroyed.
    gui(cid).get_nw_window(function(nw_win) {
        if (cid !== "nobody") {
            nw_win.window.nw_undo_menu(undo_text, redo_text);
        }
    });
}

function zoom_level_to_chrome_percent(nw_win) {
    var zoom = nw_win.zoomLevel;
    switch (zoom) {
        case -7:
            zoom = 4;
            break;
        case -6:
            zoom = 100/33;
            break;
        case -5:
            zoom = 2;
            break;
        case -4:
            zoom = 100/67;
            break;
        case -3:
            zoom = 100/75;
            break;
        case -2:
            zoom = 100/80;
            break;
        case -1:
            zoom = 100/90;
            break;
        case 0:
            zoom = 1;
            break;
        case 1:
            zoom = 100/110;
            break;
        case 2:
            zoom = 100/125;
            break;
        case 3:
            zoom = 100/150;
            break;
        case 4:
            zoom = 100/175;
            break;
        case 5:
            zoom = 100/200;
            break;
        case 6:
            zoom = 100/250;
            break;
        case 7:
            zoom = 100/300;
            break;  
    }
    return zoom;
}

// ico@vt.edu 2021-10-17: added delayed version to allow for the
// new window to open before making the call. 1000ms is an approximation
// and may not work on slow machines. LATER: think about how to deal
// with the back-end (or front-end) being unable to know when everything
// has finished loading... Maybe look for a hook when all of the binbuf
// objects have been loaded and somehow ensure that the last object
// reports finishing drawing? What about adding it a unique class just
// for loading purposes?
// ico@vt.edu 2021-10-21: perhaps the readyState solves the problem?
var delayed_scroll = {};

function gui_canvas_delayed_scroll_to_gobj(cid, tag, smooth) {
    if (!patchwin[cid] || patchwin[cid].window.document.readyState !== "complete")
    {
        if (delayed_scroll[cid] && delayed_tag[cid] != tag) {
            //post("delayed_scroll_to_gobj cancelling..." + delayed_tag[cid]);
            clearTimeout(delayed_scroll[cid]);
            delayed_scroll[cid] = null;
        }
        //post("delayed_scroll_to_gobj delaying..." + tag);
        delayed_scroll[cid] = setTimeout(gui_canvas_delayed_scroll_to_gobj, 1000, cid, tag, smooth);
    } else {
        //post("delayed_scroll_to_gobj executing..." + tag);
        setTimeout(gui_canvas_scroll_to_gobj, 100, cid, tag, smooth);
        delayed_scroll[cid] = null;
        //gui_canvas_scroll_to_gobj(cid, tag, smooth);
    }
}

exports.gui_canvas_delayed_scroll_to_gobj = gui_canvas_delayed_scroll_to_gobj;

// ico@vt.edu 2021-04-20: used to autoscroll after doing the "find" request
// on the main patch canvas
function gui_canvas_scroll_to_gobj(cid, tag, smooth) {
    //post("gui_canvas_scoll_to_gobj " + tag);
    if (delayed_scroll[cid]) {
        //post("+++ scroll_to_gobj cancelling..." + delayed_tag[cid]);
        clearTimeout(delayed_scroll[cid]);
        delayed_scroll[cid] = null;
    }
    //post("\n"+tag+"\ngui_canvas_scroll_to_gobj");
    //post("gui_canvas_scroll_to_gobj " +
    //    (patchwin[cid] == null ? "not yet" : "got it"));
    if (patchwin[cid] !== null) {
        var gobj = null;
        try {
            gobj = get_gobj(cid, tag);
        } catch(error) {

        }
        if (gobj !== null) {
            //post("gobj="+gobj+" tag="+tag);
            var x1, y1, x2, y2;
            var bbox = gobj.getBBox();
            x1 = gobj.getCTM().e;
            y1 = gobj.getCTM().f;
            //post("..."+cid+" BBOX: x1="+x1+" y1="+y1+" x2="+x2+" y2="+y2);
            gui(cid).get_nw_window(function(nw_win) {
                var svg_elem = nw_win.window.document.getElementById("patchsvg");
                var { x: x, y: y, w: width, h: height,
                    mw: min_width, mh: min_height } = canvas_params(nw_win, cid);
                /*
                post("..."+cid+" x="+x+" y="+y+" w="+width+" h="+height+
                  " mw="+min_width+" mh="+min_height);
                post("..."+cid+" scrollX="+nw_win.window.scrollX+
                  " scrollY="+nw_win.window.scrollY);
                */
                // ico@vt.edu 2021-10-21:
                // if the left/top edge of the canvas is less than 0 (x or y),
                // we need to add that from the reported x and y position
                // from the object's BBOX calculated above. Given the values
                // for x and y are negative, we will be effectively subtracting.
                if (x < 0)
                    x1 = x1 + x;
                if (y < 0)
                    y1 = y1 + y;
                x2 = x1 + bbox.width;
                y2 = y1 + bbox.height;
                //post("..."+cid+" adjusted BBOX: x1="+x1+" y1="+y1+" x2="+x2+" y2="+y2);
                var tlx, tly, brx, bry; // top-left x and y, bottom-right x and y
                var offsetx = 0, offsety = 0; // final offset

                tlx = x + nw_win.window.scrollX;
                tly = y + nw_win.window.scrollY;
                brx = tlx + min_width;
                bry = tly + min_height;
                //post("("+tlx+","+tly+") | ("+brx+","+bry+")");

                if (x2 - brx > 0)
                    offsetx = x2 - brx + 30;
                else if (x1 - tlx < 0)
                    offsetx = x1 - tlx - 30;

                if (y2 - bry > 0)
                    offsety = y2 - bry + 30;
                else if (y1 - tly < 0)
                    offsety = y1 - tly - 30;

                //post("..."+cid+" final scroll x:"+offsetx+" y:"+offsety + " | zoom=" + patchwin[cid].zoomLevel);
                var zoom = patchwin[cid].zoomLevel;
                nw_win.window.scrollBy({
                    left: offsetx,
                    top: offsety,
                    behavior: (smooth === 1 ? 'smooth' : 'auto')
                });
            });
        }
    }
}

exports.gui_canvas_scroll_to_gobj = gui_canvas_scroll_to_gobj;


// leverages the get_nw_window method in the callers...
// we need to pass both nw_win and cid because the two
// are not the same
function canvas_params(nw_win, cid)
{
    // calculate the canvas parameters (svg bounding box and window geometry)
    // for do_getscroll and do_optimalzoom
    //post("canvas_params nw_win=" + nw_win + " " + nw_win.window + " " + nw_win.window.document);
    var bbox, width, height, min_width, min_height, x, y,
        selbox, patchcord, svg_elem, gop_svgs, i, k12m;
    svg_elem = nw_win.window.document.getElementById("patchsvg");
    k12m = nw_win.window.document.getElementById("k12_menu");
    // ico@vt.edu 20210421: hide selection box, so that we don't include it in the
    // bbox calculation
    selbox = nw_win.window.document.getElementById("selection_rectangle");
    patchcord = nw_win.window.document.getElementById("newcord");
    if(selbox)
        selbox.style.display = "none";
    if(patchcord)
        patchcord.style.display = "none";

    // ico@vt.edu 20200915: hide all GOP objects in case they spill over to get us
    // accurate bbox values...
    gop_svgs = nw_win.window.document.getElementsByClassName("graphsvg");
    if (gop_svgs.length > 0) {
        for (i = 0; i < gop_svgs.length; i++)
        {
            gop_svgs[i].style.display = "none";
        }
    }
    // ...now get bbox...
    bbox = svg_elem.getBBox();
    // ... now make invised objects visible
    if(selbox)
        selbox.style.display = "block";
    if(patchcord)
        patchcord.style.display = "block";
    // ...and now make all the bbox objects visible again
    if (gop_svgs.length > 0) {
        for (i = 0; i < gop_svgs.length; i++)
        {
            gop_svgs[i].style.display = "block";
        }
    }
    //post("canvas_params calculated bbox: x=" + bbox.x +
    //     " w=" + bbox.width + " offset=" + k12_menu_canvas_x[cid]);
    // ico@vt.edu 2022-12: adjust canvas size if the k12_menu
    // is unfolded.
    if ((k12_mode == 1 || k12_menu_vis == 1)) {
        if (k12m.style.left == "0px") {
            if (k12_menu_canvas_x[cid] == null) {
                //post("set offset");
                // if there are no objects on canvas, reset stored offset
                if (bbox.x == 0 && bbox.width == 0)
                    k12_menu_canvas_x[cid] = 0;
                //post("canvas params--expanding the window size");
                if (!k12_menu_canvas_x[cid] ||
                    ((bbox.x < 0 ? bbox.x : 0)  - 155) < k12_menu_canvas_x[cid])
                    k12_menu_canvas_x[cid] = (bbox.x < 0 ? bbox.x : 0)  - 155;
            }
        } else {
            k12_menu_canvas_x[cid] = null;
        }
    } else {
        k12_menu_canvas_x[cid] = null;
    }
    if (k12_menu_canvas_x[cid] !== null) {
        bbox.width += 155;
        bbox.x = k12_menu_canvas_x[cid];
    }
    //post("...post k12 menu canvas_params calculated bbox: x=" +
    //     bbox.x + " w=" + bbox.width);
    // We try to do Pd-extended style canvas origins. That is, coord (0, 0)
    // should be in the top-left corner unless there are objects with a
    // negative x or y.
    // To implement the Pd-l2ork behavior, the top-left of the canvas should
    // always be the topmost, leftmost object.
    width = bbox.x > 0 ? bbox.x + bbox.width : bbox.width;
    // ico@vt.edu 2020-08-12: we add 1 due to an unknown nw.js discrepancy,
    // perhaps because of rounding taking place further below?
    height = bbox.y > 0 ? bbox.y + bbox.height + 1 : bbox.height + 1;
    x = bbox.x > 0 ? 0 : bbox.x,
    y = bbox.y > 0 ? 0 : bbox.y;

    // ico@vt.edu: adjust body width and height to match patchsvg to ensure
    // scrollbars only come up when we are indeed inside svg and not before
    // with extra margins around. This is accurate to a pixel on nw 0.47.0.
    // This is needed when maximizing and restoring the window in order
    // to trigger resizing of scrollbars. This value reflects the pre-zoom
    // size but this is good enough for detecting window resizing changes.

    // ico @vt.edu 2020-08-13 UPDATE: I tracked down the inconsistency in
    // measuring window size between Windows and OSX/Linux and it boils down
    // to innerWidth and innerHeight for some reason giving out inconsistent
    // values. For this reason, I have added the checks in the index.js'
    // nw_create_window, and the pdgui.js' canvas_check_geometry.
    min_width = nw_win.window.innerWidth + 1.5;
    min_height = nw_win.window.innerHeight + 0.5;
    
    var body_elem = nw_win.window.document.body;
    body_elem.style.width = min_width + "px";
    body_elem.style.height = min_height + "px";
    //post("canvas_params min_w=" + min_width + " min_h=" + min_height);

    // Since we don't do any transformations on the patchsvg,
    // let's try just using ints for the height/width/viewBox
    // to keep things simple.
    width |= 0; // drop everything to the right of the decimal point
    height |= 0;
    min_width |= 0;
    min_height |= 0;
    x |= 0;
    y |= 0;

    /* ico@vt.edu: now let's draw/update our own scrollbars, so that we
       don't have to deal with the window size nonsense caused by the
       built-in ones... */  
    // zoom var is used to compensate for the zoom level and keep
    // the scrollbars the same height
    var zoom = zoom_level_to_chrome_percent(nw_win);
    var yScrollSize, yScrollTopOffset;
    var vscroll = nw_win.window.document.getElementById("vscroll");
    yScrollSize = (min_height + nw_version_bbox_offset) / height; // used to be (min_height - 1) / height
    yScrollTopOffset = Math.floor((nw_win.window.scrollY / height) * (min_height + 3) - 1);
    
    // yScrollSize reflects the amount of the patch we currently see,
    // so if it drops below 1, that means we need our scrollbars 
    if (yScrollSize < 1 && nw_win.window.document.body.style.overflow == "visible") {
        var yHeight = Math.floor(yScrollSize * min_height);
        vscroll.style.setProperty("height", (yHeight - 1 + nw_version_bbox_offset) + "px");
        // was (yScrollTopOffset + 2) to make it peel away from the edge
        vscroll.style.setProperty("top", (yScrollTopOffset + 0) + "px");
        vscroll.style.setProperty("-webkit-clip-path",
            "polygon(0px 0px, 5px 0px, 5px " + (yHeight - 1 + nw_version_bbox_offset) +
            "px, 0px " + (yHeight - 6 + nw_version_bbox_offset) + "px, 0px 5px)");
        // ico@vt.edu: this could go either way. We can zoom here to compensate for
        // the zoom and keep the scrollbars the same size, or, as is the case with
        // this new commit, we enlarge them together with the patch since one of the
        // possible rationales is that zooming is there to improve visibility. If
        // we decide to reenable this, we may want to fine-tune scrollbar height to
        // ensure its size is accurate.
        //vscroll.style.setProperty("width", (5 * zoom) + "px");
        //vscroll.style.setProperty("right", (2 * zoom) + "px");
        vscroll.style.setProperty("visibility", "visible");
    } else {
        vscroll.style.setProperty("visibility", "hidden");
    }
    
    var xScrollSize, xScrollLeftOffset;
    var hscroll = nw_win.window.document.getElementById("hscroll");
    xScrollSize = (min_width + nw_version_bbox_offset) / width; // used to be (min_width - 1) / width
    xScrollLeftOffset = Math.floor((nw_win.window.scrollX / width) * (min_width + 3) - 1);
    //post("body_scroll=" + (nw_win.window.document.body.style.overflow));

    if (xScrollSize < 1 && nw_win.window.document.body.style.overflow == "visible") {
        var xWidth = Math.floor(xScrollSize * min_width);
        hscroll.style.setProperty("width", (xWidth - 1) + "px");
        // was (xScrollTopOffset + 2) to make it peel away from the edge
        hscroll.style.setProperty("left", (xScrollLeftOffset + 0) + "px");
        hscroll.style.setProperty("-webkit-clip-path",
            "polygon(0px 0px, " + (xWidth - 6) + "px 0px, " +
            (xWidth - 1) + "px 5px, 0px 5px)");
        // ico@vt.edu: this could go either way. We can zoom here to compensate for
        // the zoom and keep the scrollbars the same size, or, as is the case with
        // this new commit, we enlarge them together with the patch since one of the
        // possible rationales is that zooming is there to improve visibility. If
        // we decide to reenable this, we may want to fine-tune scrollbar width to
        // ensure its size is accurate.
        //hscroll.style.setProperty("height", (5 * zoom) + "px");
        //hscroll.style.setProperty("bottom", (2 * zoom) + "px");
        hscroll.style.setProperty("visibility", "visible");
    } else {
        hscroll.style.setProperty("visibility", "hidden");    
    }
    
    //post("x=" + xScrollSize + " y=" + yScrollSize);
    //post("canvas_params final: x=" + x + " y=" + y + "w=" + width +
    //  " h=" + height + " min_w=" + min_width + " min_h=" + min_height);
    
    return { x: x, y: y, w: width, h: height,
             mw: min_width, mh: min_height };
}


// ico@vt.edu:
// the timeout is a bad hack and does not solve the problem consistently
// even on a fast computer, while also slowing down the overall user 
// experience. As such, ti is disabled and left here for reference.
/*var pd_getscroll_var = {};

function pd_do_getscroll(cid) {
    if (!pd_getscroll_var[cid]) {
        pd_getscroll_var[cid] = setTimeout(function() {
            do_getscroll(cid, 0);
            pd_getscroll_var[cid] = null;
        }, 250);
    }
}

exports.pd_do_getscroll = pd_do_getscroll;*/

// ico@vt.edu: we need this because of inconsistent canvas size between
// nw <=0.24 and >=0.46
var nw_version_bbox_offset = check_nw_version("0.46") ? 0 : -4;

function do_getscroll(cid, checkgeom) {
    // Since we're throttling these getscroll calls, they can happen after
    // the patch has been closed. We remove the cid from the patchwin
    // object on close, so we can just check to see if our Window object has
    // been set to null, and if so just return.
    // This is an awfully bad pattern. The whole scroll-checking mechanism
    // needs to be rethought, but in the meantime this should prevent any
    // errors wrt the rendering context disappearing.
    //post("do_getscroll " + checkgeom);
    if (checkgeom == 1) {
        canvas_check_geometry(cid);
        return;
    }
    gui(cid).get_nw_window(function(nw_win) {
        var svg_elem = nw_win.window.document.getElementById("patchsvg");
        var { x: x, y: y, w: width, h: height,
            mw: min_width, mh: min_height } = canvas_params(nw_win, cid);

        //post("nw_version_bbox_offset=" + nw_version_bbox_offset +
        //  " min_height=" + min_height);
        min_height += nw_version_bbox_offset;
        //post("post-calc min_height=" + min_height);

        if (width < min_width) {
            width = min_width;
        }
        // If the svg extends beyond the viewport, it might be nice to pad
        // both the height/width and the x/y coords so that there is extra
        // room for making connections and manipulating the objects. As it
        // stands objects will be flush with the scrollbars and window
        // edges.
        if (height < min_height) {
            height = min_height;
        }
        configure_item(svg_elem, {
            viewBox: [x, y, width, height].join(" "),
            width: width,
            height: height
        });
        // Now update the svg's background if we're in edit mode. This adds
        // a new background image to the body of the document each time.
        // So if there is a performance regression with do_getscroll when
        // in editmode, this could be the culprit.
        update_svg_background(cid, svg_elem);
    });
}

exports.do_getscroll = do_getscroll;

var getscroll_var = {};
var checkgeom_and_getscroll_var = {};
var overriding_getscroll_var = {};

// We use a setTimeout here for two reasons:
// 1. nw.js has a nasty Renderer bug when you try to modify the
//    window before the document has finished loading. To get
//    the error get rid of the setTimeout
// 2. This should protect the user from triggering a bunch of
//    layouts.  But this only works because I'm not updating
//    the view to follow the mouse-- for example, when
//    the user is dragging an object beyond the bounds of the
//    viewport. The tcl/tk version actually does follow the
//    mouse. In that case this setTimeout could keep the
//    graphics from displaying until the user releases the mouse,
//    which would be a buggy UI
function gui_canvas_get_scroll(cid) {
    //post("gui_canvas_get_scroll");
    //post("win=" + cid);
    //win_width = win.style.width;
    //win_height = win.style.height;
    if (toplevel_scalars[cid] === 1) {
        // we have scalars, so let's override the previous call
        // because this will be a cpu intensive redraw, so we
        // should do it only when the action that requested it
        // is either done or stopped for long enough for the
        // recalculation to happen
        if (getscroll_var[cid]) {
            clearTimeout(getscroll_var[cid]);
            getscroll_var[cid] = null;
        }
    }
    if (!getscroll_var[cid]) {
        getscroll_var[cid] = setTimeout(function() {              
            do_getscroll(cid, toplevel_scalars[cid]);
            getscroll_var[cid] = null;
        }, 50);
    }
}

exports.gui_canvas_get_scroll = gui_canvas_get_scroll;

function gui_canvas_get_scroll_on_resize(cid) {
    //post("gui_canvas_get_scroll_on_resize " + toplevel_scalars[cid]);
    if (toplevel_scalars[cid] === 2) {
        // ico@vt.edu 2021-11-12: if we have toplevel scalars
        // that require scaling on window resize, we deal with that
        // here
        if (getscroll_var[cid]) {
            clearTimeout(getscroll_var[cid]);
            getscroll_var[cid] = null;
        };
        if (!getscroll_var[cid]) {
            getscroll_var[cid] = setTimeout(function() {  
                //post("...requesting redraw");            
                do_getscroll(cid, 1);
                //pdsend(cid, "redraw");
                getscroll_var[cid] = null;
            }, 50);
        }        
    } else {
        gui_canvas_get_scroll(cid);
    }
}

exports.gui_canvas_get_scroll_on_resize = gui_canvas_get_scroll_on_resize;


/* ico@vt.edu: here is one alternative getscroll call, it focuses on
   overriding the previous call, so the getscroll is more delayed. This
   is useful when manipulating a plot with a mouse, for instance, so that
   we prevent excessive getscroll calls which can be rather cpu intensive.
*/

function gui_canvas_get_overriding_scroll(cid) {
    //post("gui_canvas_get_overriding_scroll");
    //post("win=" + cid);
    //win_width = win.style.width;
    //win_height = win.style.height;
    if (overriding_getscroll_var[cid]) {
        clearTimeout(overriding_getscroll_var[cid]);
    }
    overriding_getscroll_var[cid] = setTimeout(function() {
        do_getscroll(cid, 0);
        overriding_getscroll_var[cid] = null;
    }, 100);
}

exports.gui_canvas_get_overriding_scroll = gui_canvas_get_overriding_scroll;

/* ico@vt.edu 20200920: this last variant that executes immediately
   is needed for g_text.c when one displaces a text object and it
   immediately activates and it falls outside the visible canvas bounds
   this can trigger the object to have its activated box at an incorrect
   location due to asynchronous behavior of other getscroll calls. Having
   it here as a separate call as it may prove useful later in other contexts.
*/

function gui_canvas_get_immediate_scroll(cid) {
    //post("gui_canvas_get_immediate_scroll");
    do_getscroll(cid, 0);
}

exports.gui_canvas_get_immediate_scroll = gui_canvas_get_immediate_scroll;

function do_optimalzoom(cid, hflag, vflag) {
    // determine an optimal zoom level that makes the entire patch fit within
    // the window
    gui(cid).get_nw_window(function(nw_win) {
        var { x: x, y: y, w: width, h: height, mw: min_width, mh: min_height } =
            canvas_params(nw_win, cid);
        // Calculate the optimal horizontal and vertical zoom values,
        // using floor to always round down to the nearest integer. Note
        // that these may well be negative, if the viewport is too small
        // for the patch at the current zoom level. XXXREVIEW: We assume a
        // zoom factor of 1.2 here; this works for me on Linux, but I'm
        // not sure how portable it is. -ag
        var zx = 0, zy = 0;
        if (width>0) zx = Math.floor(Math.log(min_width/width)/Math.log(1.2));
        if (height>0) zy = Math.floor(Math.log(min_height/height)/Math.log(1.2));
        // Optimal zoom is the minimum of the horizontal and/or the vertical
        // zoom values, depending on the h and v flags. This gives us the offset
        // to the current zoom level. We then need to clamp the resulting new
        // zoom level to the valid zoom level range of -7..+7.
        var actz = nw_win.zoomLevel, z = 0;
        if (hflag && vflag)
            z = Math.min(zx, zy);
        else if (hflag)
            z = zx;
        else if (vflag)
            z = zy;
        z += actz;
        if (z < -7) z = -7; if (z > 7) z = 7;
        //post("bbox: "+width+"x"+height+"+"+x+"+"+y+" window size: "+min_width+"x"+min_height+" current zoom level: "+actz+" optimal zoom level: "+z);
        if (z != actz) {
            nw_win.zoomLevel = z;
            pdsend(cid, "zoom", z);
        }
        do_getscroll(cid,1);
    });
}

var optimalzoom_var = {};

// We use a setTimeout here as with do_getscroll above, but we have to
// use a smaller value here, so that we're done before a subsequent
// call to do_getscroll updates the viewport. XXXREVIEW: Hopefully
// 100 msec are enough for do_optimalzoom to finish.
function gui_canvas_optimal_zoom(cid, h, v) {
    clearTimeout(optimalzoom_var[cid]);
    optimalzoom_var[cid] = setTimeout(do_optimalzoom, 50, cid, h, v);
}

exports.gui_canvas_optimal_zoom = gui_canvas_optimal_zoom;

// handling the selection
function gui_lower(cid, tag) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        var first_child = svg_elem.firstElementChild,
        first_cord = svg_elem.querySelector(".cord"),
        child = first_child,
        selection = [],
        gobj,
        len,
        i;
        if (tag === "selected") {
            while (child !== null && child !== first_cord) {
                if (child.classList.contains("selected")) {
                    selection.push(child);
                }
                child = child.nextElementSibling;
            }
            //selection = svg_elem.getElementsByClassName("selected");
        } else {
            gobj = get_gobj(cid, tag);
            if (gobj !== null) {
                selection = [gobj];
            }
        }
        if (selection.length > 0) {
            len = selection.length;
            for (i = 0; i < len; i++) {
                svg_elem.insertBefore(selection[i], first_child);
            }
        }
    });
}

// This only differs from gui_raise by setting first_child to
// the cord element instead of the first element in the svg.  Really,
// all three of these should be combined into a single function (plus
// all the silly logic on the C side moved here
function gui_raise(cid, tag) {
    gui(cid).get_elem("patchsvg", function(svg_elem) {
        var first_child = svg_elem.firstElementChild,
        first_cord = svg_elem.querySelector(".cord"),
        child = first_child,
        selection = [],
        gobj,
        len,
        i;
        if (tag === "selected") {
            while (child !== null && child !== first_cord) {
                if (child.classList.contains("selected")) {
                    selection.push(child);
                }
                child = child.nextElementSibling;
            }
            //selection = svg_elem.getElementsByClassName("selected");
        } else {
            gobj = get_gobj(cid, tag);
            if (gobj !== null) {
                selection = [gobj];
            }
        }
        if (selection.length > 0) {
            len = selection.length;
            for (i = 0; i < len; i++) {
                svg_elem.insertBefore(selection[i], first_cord);
            }
        }
    });
}

function gui_find_lowest_and_arrange(cid, reference_element_tag, objtag) {
    gui(cid).get_gobj(reference_element_tag, function(ref_elem, w) {
        var svg_elem = w.document.getElementById("patchsvg"),
        selection = null,
        gobj,
        len,
        i; 
        if (objtag === "selected") {
            selection = svg_elem.getElementsByClassName("selected");
        } else {
            gobj = get_gobj(cid, objtag);
            if (gobj !== null) {
                selection = [get_gobj(cid, objtag)];
            }
        }
        if (selection !== null) {
            len = selection.length;
            for (i = len - 1; i >= 0; i--) {
                svg_elem.insertBefore(selection[i], ref_elem);
            }
        }
    });
}

// Bindings for dialog menu of iemgui, canvas, etc.
exports.dialog_bindings = function(did) {
    var dwin = dialogwin[did].window;
    dwin.document.onkeydown = function(evt) {
        //post("onkeydown=" + evt.keyCode + " ctrl=" +
        //    evt.ctrlKey + " meta=" + evt.metaKey);
        if (!nw_os_is_osx && evt.keyCode == 13 && evt.ctrlKey ||
             nw_os_is_osx && evt.keyCode == 13 && evt.metaKey) {
            // cmd/ctrl+enter/return
            // ico@vt.edu 2022-12-03:
            // activate ok only if we use ctrl + enter
            // this is because if we are inside a
            // dropdown menu nwjs may crash
            // because it is trying to create a
            // dropdown at a time the window is closing.
            //var element = dwin.document.activeElement;
            //var tagName = element.tagName.toLowerCase();
            //post("dialog detected enter keypress on element id " + tagName);
            //if (tagName !== 'select') {
            dwin.ok();
            //}
        } else if (evt.keyCode === 27) { // escape
            dwin.cancel();
        } else if (!nw_os_is_osx && evt.keyCode == 65 && evt.ctrlKey ||
                    nw_os_is_osx && evt.keyCode == 65 && evt.metaKey) {
            // cmd/ctrl+a
            //post("onkeydown ctrl+a");
            var element = dwin.document.activeElement;
            if (element) {
                var tagName = element.tagName.toLowerCase();
                if (tagName === 'input') {
                    //post("...got input");
                    var type = element.getAttribute('type').toLowerCase();
                    if (type === "text" || type === "number")
                    {
                        //post("...we got text or number, selecting")
                        element.select();
                    }
                }
                evt.preventDefault();
            }
        }
        else if (nw_os_is_osx && evt.keyCode == 87 && evt.metaKey) {
            //post("OSX version of cmd+w");
            evt.preventDefault();
            dwin.cancel();
        }
    };
    dwin.document.onkeypress = function(evt) {
        //post("onkeypress " + evt.which + " " + evt.ctrlKey);
        if (!nw_os_is_osx && evt.which == 23 && evt.ctrlKey) {
            //post("Ctrl+W pressed");
            evt.preventDefault();
            dwin.cancel();
        }
    }
}

exports.fix_window_size = function(did) {
    //ico@vt.edu: comment the following line when working on dialog sizes...
    dialogwin[did].setResizable(false);
}

exports.resize_window = function(did) {
    var w = dialogwin[did].window.document.body.scrollWidth,
        h = dialogwin[did].window.document.body.scrollHeight;
    // ico@vt.edu: the following is a change needed for the nw.js 0.47
    // for the dialog window to be properly resized
    //dialogwin[did].width = w;
    //dialogwin[did].height = h;
    //dialogwin[did].window.document.body.titlebar_close_button.style.setProperty
    //    ("font-size", (process.platform === "win32" ? "21px" : "15px"));
    /*post(did + " body: w=" + dialogwin[did].window.document.body.clientWidth +
        " h=" + dialogwin[did].window.document.body.clientHeight + " scroll: w=" +
        w + " h=" + h);*/
    dialogwin[did].resizeTo(w,h);
    //ico@vt.edu: comment the following line when working on dialog sizes...
    dialogwin[did].setResizable(false);
    //post("dialog set always on top");
    //dialogwin[did].setAlwaysOnTop(true);
}

// External GUI classes

function gui_pddplink_open(filename, dir) {
    var full_path, revised_dir, revised_filename;
    if (filename.indexOf("://") > -1) {
        external_doc_open(filename);
    } else if (path_is_absolute(filename)) {
        doc_open(path.dirname(filename), path.basename(filename));
    } else if (fs.existsSync(path.join(dir, filename))) {
        full_path = path.normalize(path.join(dir, filename));
        revised_dir = path.dirname(full_path);
        revised_filename = path.basename(full_path);
        doc_open(revised_dir, revised_filename);
    } else {
        // Give feedback to let user know the link didn't work...
        post("pddplink: error: file not found: " + filename);
    }
}

/* ico@vt.edu: this function is run when we scroll with a mouse wheel,
   a touchpad (e.g. two-finger scroll), or some other HID. It is
   linked from the pd_canvas.js and called from the garray_fittograph 1
   call when we are resizing the plot toplevel window to avoid race condition.
*/
function gui_update_scrollbars(cid) {
    //post("gui_update_scrollbars " + cid);
    gui(cid).get_nw_window(function(nw_win) {
        var hscroll = nw_win.window.document.getElementById("hscroll");
        var vscroll = nw_win.window.document.getElementById("vscroll");
        var svg_elem = nw_win.window.document.getElementById("patchsvg");

        if (patchwin[cid].window.document.body.style.overflow != "visible")
        {
            vscroll.style.setProperty("visibility", "hidden");
            hscroll.style.setProperty("visibility", "hidden");
        } else {        
            if (vscroll.style.visibility == "visible")
            { 
                var min_height = nw_win.window.innerHeight + 0.5;
                var height = svg_elem.getAttribute('height');
                var yScrollSize, yScrollTopOffset;

                height |= 0;  // drop everything to the right of the decimal point
                min_height |= 0;
                
                yScrollSize = (min_height + nw_version_bbox_offset) / height;
                yScrollTopOffset = Math.floor((nw_win.window.scrollY / height) * min_height);
                
                if (yScrollSize < 1) {
                    var yHeight = Math.floor(yScrollSize * min_height);
                    vscroll.style.setProperty("height", (yHeight - 1 + nw_version_bbox_offset) + "px");
                    vscroll.style.setProperty("top", (yScrollTopOffset + 0) + "px");
                    vscroll.style.setProperty("-webkit-clip-path",
                        "polygon(0px 0px, 5px 0px, 5px " + (yHeight - 1 + nw_version_bbox_offset) +
                        "px, 0px " + (yHeight - 6 + nw_version_bbox_offset) + "px, 0px 5px)");
                    vscroll.style.setProperty("visibility", "visible");
                } else {
                    vscroll.style.setProperty("visibility", "hidden");    
                }
            }

            if (hscroll.style.visibility == "visible")
            {
                var min_width = nw_win.window.innerWidth + 1.5;
                var width = svg_elem.getAttribute('width');
                var xScrollSize, xScrollTopOffset;

                width |= 0; // drop everything to the right of the decimal point
                min_width |= 0;
                
                xScrollSize = (min_width + nw_version_bbox_offset) / width;
                xScrollTopOffset = Math.floor((nw_win.window.scrollX / width) * min_width);
                
                /* console.log("win_width=" + min_width + " bbox=" +
                    width + " xScrollSize=" + (xScrollSize * min_width) +
                    " topOffset=" + xScrollTopOffset); */

                if (xScrollSize < 1) {
                    var xWidth = Math.floor(xScrollSize * min_width);
                    hscroll.style.setProperty("width", (xWidth - 1) + "px");
                    hscroll.style.setProperty("left", (xScrollTopOffset + 0) + "px");
                    hscroll.style.setProperty("-webkit-clip-path",
                        "polygon(0px 0px, " + (xWidth - 6) + "px 0px, " +
                        (xWidth - 1) + "px 5px, 0px 5px)");
                    hscroll.style.setProperty("visibility", "visible");
                } else {
                    hscroll.style.setProperty("visibility", "hidden");    
                }
            }
            // for future reference
            //nw_win.document.getElementById("hscroll").
            //    style.setProperty("visibility", "visible");
            //console.log("width="+width);
        }
    });
}

exports.gui_update_scrollbars = gui_update_scrollbars;

// ico@vt.edu 2020-08-29: fine-tune appearance of various
// css elements because, consistency in HTML font rendering
// across different OSs is a joke
function gui_check_for_dialog_appearance_inconsistencies(id)
{
    if (nw_os_is_osx)
        gui_osx_dialog_appearance(id);
}

exports.gui_check_for_dialog_appearance_inconsistencies = gui_check_for_dialog_appearance_inconsistencies;

function gui_osx_dialog_appearance(id)
{
    var close_button = dialogwin[id].window.document.getElementById("titlebar_close_button");
    close_button.style.setProperty("line-height", "14px");
    close_button.style.setProperty("border-radius", "10px");
}

// functions for copying apps folder into the user folder
var fs = require('fs');
var path = require('path');

function copyFileSync( source, target ) {

    var targetFile = target;

    // If target is a directory, a new file with the same name will be created
    if ( fs.existsSync( target ) ) {
        if ( fs.lstatSync( target ).isDirectory() ) {
            targetFile = path.join( target, path.basename( source ) );
        }
    }

    fs.writeFileSync(targetFile, fs.readFileSync(source));
}

function copyFolderRecursiveSync( source, target ) {
    var files = [];

    // Check if folder needs to be created or integrated
    var targetFolder = path.join( target, path.basename( source ) );
    if ( !fs.existsSync( targetFolder ) ) {
        fs.mkdirSync( targetFolder );
    }

    // Copy
    if ( fs.lstatSync( source ).isDirectory() ) {
        files = fs.readdirSync( source );
        files.forEach( function ( file ) {
            var curSource = path.join( source, file );
            if ( fs.lstatSync( curSource ).isDirectory() ) {
                copyFolderRecursiveSync( curSource, targetFolder );
            } else {
                copyFileSync( curSource, targetFolder );
            }
        } );
    }
}

function restore_apps(force) {
    const fs = require('fs');
    var homedir, dir;
    if (nw_os_is_windows) {
        homedir = process.env.USERPROFILE;
    } else {
        homedir = process.env.HOME;
    }

    if (nw_os_is_windows) {
        dir = homedir + '\\Pd-L2Ork';
    } else {
        dir = homedir + '/Pd-L2Ork';
    }
    //post("dir=" + dir + "\nprocess.cwd=" + process.cwd());
    if (force == 0 && fs.existsSync(dir)) {
        post("User app directory found. Skipping copying of the Pd-L2Ork apps into the " +
                   "user folder. If you would like to force restore the apps to the latest " +
                   "version included with this release of Pd-L2Ork, open the General tab " +
                   "found in the Preferences, and click on the Restore User Apps Folder button...");
    } else {
        post("Restoring user apps folder...");
        if (force == 0)
            post("User app directory not found...\nCreating directory " + dir + "...");
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, '0755');
        }
        if (force == 0) post("Copying Pd-L2Ork Apps...");
        if (nw_os_is_osx) {
            // ico@vt.edu 2021-09-10:
            // here we need to reference lib_dir which is initialized inside index.js
            // otherwise process.cwd() when invoked from the preferences window
            // returns the user's home folder which results in a failed copy operation
            // this does not affect Windows, at least not on the current version of nw.js
            // LATER: check this against future nw.js versions
            if (force == 1)
                copyFolderRecursiveSync(lib_dir+"/apps/", dir);
            else
                copyFolderRecursiveSync(process.cwd()+"/apps/", dir);
        } else {
            copyFolderRecursiveSync(process.cwd()+"/../apps/", dir);
        }
        post("Done!")
    }
}

exports.restore_apps = restore_apps;

// ico@vt.edu 2021-08-20: variable to inform the pdgui when the object
// has been grabbed. We use this to enable pasting of text inside gatoms
// LATER: consider using this for other purposes, as well
var gobj_grabbed = {};

// ico 2023-11-29: way to keep global state of modifiers
// so that when we release focus we send the latest state
// to pd (this is an issue if an object was focused with
// exclusive focus enabled, in which case, the release
// of these modifiers may be passed only onto the focused
// object and pd will then be stuck thinking that they
// continue to be pressed)
var grab_shift = 0;
var grab_ctrl = 0;
var grab_alt = 0;
var grab_exclusive = 0;

function gui_gobj_grabbed(cid, val, exclusive, ctrl, alt, shift) {
    gobj_grabbed[cid] = val;
    //post("gui_gobj_grabbed "+cid+" "+gobj_grabbed[cid]);
    //post("gui_gobj_grabbed " + val + " shift=" + grab_shift
    //     + " ctrl=" + grab_ctrl + " alt=" + grab_alt);
    if (val) {
        grab_ctrl = ctrl;
        grab_alt = alt;
        grab_shift = shift;
        grab_exclusive = exclusive;
    }
    // now send fake ctrl + alt + shift command to pd
    // to ensure its awareness of keypresses is in sync
    // this is because if an object like gatom has had
    // an exclusive focus, then the shift keyup would
    // have been sent only to that object, not the entire
    // pd.
    if (!val && grab_exclusive) {
        if (grab_alt !== alt) {
            if (alt === 1) {
                pdsend(cid, "key", 1, "Alt", shift, 1, 0);
            } else {
                pdsend(cid, "key", 0, "Alt", shift, 1, 0);
            }
        }
        if (grab_ctrl !== ctrl) {
            if (ctrl === 1) {
                pdsend(cid, "key", 1, "Control", shift, 1, 0);
            } else {
                pdsend(cid, "key", 0, "Control", shift, 1, 0);
            }
        }
        if (grab_shift !== shift) {
            if (shift === 1) {
                pdsend(cid, "key", 1, "Shift", shift, 1, 0);
            } else {
                pdsend(cid, "key", 0, "Shift", shift, 1, 0);
            }
        }
    }
}

exports.gui_gobj_grabbed = gui_gobj_grabbed;

function gui_is_gobj_grabbed(cid) {
    if (gobj_grabbed[cid] == null) {
        //post("initializing gobj_grabbed "+cid);
        gobj_grabbed[cid] = 0;
    }
    //post("gui_is_gobj_grabbed "+cid+" "+gobj_grabbed[cid]);
    return gobj_grabbed[cid];
}

exports.gui_is_gobj_grabbed = gui_is_gobj_grabbed;


// ico@vt.edu 2021-10-17: used for find again when original find opens
// subcanvas whose canvasevents last_search_term has not yet been set
// (or is set to something else), so, we check against what the backend
// has and act accordingly.
var glob_canvas_backend_search_term = "";

exports.gui_set_backend_search_term = function(value) {
    //post("gui_set_glob_search_term="+value);
    glob_canvas_backend_search_term = value;
}

exports.gui_get_backend_search_term = function() {
    //post("gui_get_glob_search_term="+glob_canvas_backend_search_term);
    return glob_canvas_backend_search_term;
}

var glob_search_origin_canvas = "";

exports.gui_set_search_origin_canvas = function(value) {
    //post("glob_search_origin_canvas="+value);
    glob_search_origin_canvas = value;
}

exports.gui_get_search_origin_canvas = function() {
    //post("glob_search_origin_canvas="+glob_search_origin_canvas);
    return glob_search_origin_canvas;
}

function gui_close_find_bar_on_new_window_focus(cid) {
    var find_bar = patchwin[cid].window.document.getElementById("canvas_find");
    if (find_bar)
        find_bar.style.setProperty("display", "none");
}

exports.gui_close_find_bar_on_new_window_focus =
    gui_close_find_bar_on_new_window_focus;

// ico@vt.edu 2021-10-17:
// used for reseting the highlighting of objects' text
// (initiall gatom and numbox2). This is triggered exclusively
// when editing object's value using keyboard and then pressing enter.
var highlight_reset = {};

function gui_highlight_obj_on_return(cid, tag, type) {
    if (type === "gatom" || type === "numbox2") {
        //post("highlight "+tag+"text "+type);
        gui(cid).get_elem(tag + "text", function(item) {
            item.style.setProperty("font-weight", "bold");
            item.style.setProperty("text-shadow", "0.25px 0 red, -0.25px 0 red");
            if (highlight_reset[cid])
                clearTimeout(highlight_reset[cid]);
            highlight_reset[cid] =
                setTimeout(gui_highlight_obj_on_return_reset, 200, cid, tag, type);
        });
        /*
        gui(cid).get_elem(tag + "text", {
            transform: "translate(9, 12)"
        });
        */
    }
}

function gui_highlight_obj_on_return_reset(cid, tag, type) {
    if (type === "gatom" || type === "numbox2") {
        gui(cid).get_elem(tag + "text", function(item) {
            item.style.setProperty("font-weight", "normal");
            item.style.setProperty("text-shadow", "none");
        });
        /*
        gui(cid).get_elem(tag + "text", {
            transform: "translate(9, 11)"
        });
        */
    }
}

exports.gui_highlight_obj_on_return = gui_highlight_obj_on_return;

// K12 Stuff

/* returns:
     0 if it is disabled
     1 if it is enabled
*/
function get_k12_mode() {
    //post("get_k12_mode " + k12_mode);
    return k12_mode;
}

exports.get_k12_mode = get_k12_mode;

/* returns:
     0 if it is disabled
     1 if it is enabled
*/
function get_k12_menu_vis() {
    return k12_menu_vis;
}

exports.get_k12_menu_vis = get_k12_menu_vis;

// ico@vt.edu 2022-12-05: moving all this from pd_canvas.js to pdgui.js,
// so that index.js can benefit from it, as well (responsible for the
// console window).

// hlkwok@vt.edu 2022-10-12: switches between k12_mode and normal mode
function set_k12_mode(val) {
    //post("set_k12_mode " + k12_mode);
    k12_mode = val;

    // redundantly send this back to the pd engine in case this was
    // invoked via GUI
    pdsend("pd set-k12-mode " + k12_mode);
    //post("done...");

    // copied from next window logic
    var i;
    var win_array_length = Object.keys(patchwin).length;
    for (i = 0; i < win_array_length; i++) {
        if(patchwin[Object.keys(patchwin)[i]])
            patchwin[Object.keys(patchwin)[i]].window.update_menu();
    }
    // now set the main window menu
    pd_window.set_k12_mode_checkbox();
}

exports.set_k12_mode = set_k12_mode;

// hlkwok@vt.edu 2022-10-24: functionality for k12 tabs
function toggle_tab(cid, tab_id) {
    //post("toggle_tab window=" + patchwin[cid].window.document);
    if (patchwin[cid].window.document.getElementById(tab_id).style.display == "none") {
        patchwin[cid].window.document.getElementById(tab_id).style.display = "block";
    }
    else {
        patchwin[cid].window.document.getElementById(tab_id).style.display = "none";
    }
}

exports.toggle_tab = toggle_tab;

// hlkwok@vt.edu 2022-11-8: updates k12 menu height, which will adjust
// k12 menu scrollbar
function update_k12_menu(cid) {
    //post("update_k12_menu");
    var k12_menu = patchwin[cid].window.document.getElementById("k12_menu");
    var tab_menu = patchwin[cid].window.document.getElementById("tab_menu");
    // toggle k12 visibility button
    var k12_toggle = patchwin[cid].window.document.getElementById("k12_toggle");
    var toggle_height = patchwin[cid].window.document.
        getElementById("show_k12_menu").offsetHeight * 2 / 3;
    // heights for k12 menu and tab menu
    var k12_height = patchwin[cid].window.innerHeight;
    var tab_height = patchwin[cid].window.innerHeight;
    // set heights
    k12_menu.style.setProperty("height", k12_height + "px");
    tab_menu.style.setProperty("height", tab_height + "px");
    k12_toggle.style.setProperty("top", (
        -patchwin[cid].window.innerHeight / 2 - toggle_height) + "px");
}

exports.update_k12_menu = update_k12_menu;

var lock_editmode = new Image();
lock_editmode.src = "K12-icons/lock-editmode.png";

var lock_runtime = new Image();
lock_runtime.src = "K12-icons/lock-runtime.png";

// ico@vt.edu 2022-12-08:
// toggle lock flap sends its request to c (pd_canvas.js) and this is
// then invoked from gui_canvas_set_editmode above
// states:
//      1: unfolded
//      0: folded
//     -1: folded without changing the icon (used to animate disabling
//         of the k12_menu or k12_mode)
function toggle_k12_menu(cid, state) {
    //console.log("toggle_k12_menu");
    var k12m = patchwin[cid].window.document.getElementById("k12_menu");
    if (k12m.style.left != "0px" && state == 1) {
        k12m.style.left = "0px";
        patchwin[cid].window.document.
            getElementById("k12_toggle_icon").src= lock_editmode.src;
        patchwin[cid].window.resizeBy(155, 0);
    }
    else if (state == 0) {
        if (k12m.style.left == "0px")
            patchwin[cid].window.resizeBy(-155, 0);
        k12m.style.left = "-155px";
        patchwin[cid].window.document.
            getElementById("k12_toggle_icon").src= lock_runtime.src;
    }
    else if (state == -1) {
        if (k12m.style.left == "0px")
            patchwin[cid].window.resizeBy(-155, 0);
        k12m.style.left = "-175px";
    }
    gui_canvas_get_immediate_scroll(cid);
}

exports.toggle_k12_menu = toggle_k12_menu;

// ico@vt.edu 2022-12-08: toggles k12 menu visibility (for k12 menu option
// in the put menu). both k12_mode and k12_mode are global variables, so
// they should be applied to all open canvases.
function toggle_k12_menu_visibility(cid, state) {
    k12_menu_vis = state;
    var i, k12m;
    var win_array_length = Object.keys(patchwin).length;
    for (i = 0; i < win_array_length; i++) {
        // TODO see if we need a separate version of this
        // for k12_menu
        if(patchwin[Object.keys(patchwin)[i]])
            patchwin[Object.keys(patchwin)[i]].window.update_menu();
    }
}

exports.toggle_k12_menu_visibility = toggle_k12_menu_visibility;

// ico@vt.edu 2022-12-18: moving this from the dialog_prefs.html
// so that we can call it via File->Message with a nwjs: prefix
// below, we will continue to add other functions, as needed,
// and potentially eventually pull this out of this file into
// a separate js file to be included with pdgui.js.

// this first call is to reset nw.js settings, which may be
// necessary if we ever (again) downgrade nw.js, in which case
// older version may not be able to read settings of a newer
// version, thus spawning an error popup at start-up.
function reset_nwjs_user_settings_folder() {
    post("calling reset_nwjs_user_settings_folder...");
    var os = require('os').platform();
    var targetdir;
    if (nw_os_is_linux) {
        // Linux
        targetdir = path.resolve(require('os').homedir() + "/.config/pd-l2ork");
    } else if (nw_os_is_osx) {
        // OSX
        targetdir = path.resolve(require('os').homedir() + "/Library/Application Support/Pd-L2Ork");
    } else {
        // Windows
        targetdir = path.resolve(process.env.LOCALAPPDATA);
    }
    //post("targetdir=" + targetdir);
    if (fs.existsSync(targetdir)) {
        // Windows does not make this easy, so we can only inform the user of what they need to do...
        if (nw_os_is_windows) {
            post("\nWindows does not allow for deletion of the settings folder while pd-l2ork is " +
                 "running. Pd-L2Ork has just opened the folder where the settings folder is located." +
                 " Please quit pd-l2ork and then delete the pd-l2ork folder in the explorer window " +
                 "(the exact location is " + targetdir + "\\pd-l2ork). Then reopen pd-l2ork...");
            require('child_process').exec("start " + targetdir);
            return;
        }
        setTimeout(function () {
          menu_quit();
        }, 1000);
        //console.log("nwjs user settings folder exists, deleting...");
        //post("Your nwjs application folder has been deleted.");
        rmdir(targetdir);
        fs.mkdirSync(targetdir);
    }
    post("done.");
}

exports.reset_nwjs_user_settings_folder = reset_nwjs_user_settings_folder;

function reset_user_settings() {
    post("calling reset_user_settings...");
    var sourcefile, targetfile;
    // only Linux and OSX need to have their external file deleted
    // Windows is handled below via the pdsend call since it involves
    // the registry
    if (nw_os_is_osx) {
        // copy default file to user preferences
        sourcefile = path.resolve(
            global.__dirname + "/../org.puredata.pd-l2ork.default.plist");
        targetfile = path.resolve(require('os').homedir() +
            "/Library/Preferences/org.puredata.pd-l2ork.plist");
        fs.copyFile(sourcefile, targetfile, (err) => {
            if (err) throw err;
            console.log(sourcefile + " was copied to " + targetfile);
        });
    }
    else if (nw_os_is_linux) {
        // copy default file to user preferences
        sourcefile = path.resolve(global.__dirname +
            "/../default.settings");
        targetfile = path.resolve(process.env.HOME + "/.pd-l2ork/user.settings");
        //post("sourcefile=" + sourcefile + " targetfile=" + targetfile);
        fs.copyFile(sourcefile, targetfile, (err) => {
            if (err) throw err;
            console.log(sourcefile + " was copied to " + targetfile);
        });
    }
    // then handle Windows registry stuff and reinit all preferences
    pdsend("pd reinit-user-settings");
    post("done.");
}

exports.reset_user_settings = reset_user_settings;
