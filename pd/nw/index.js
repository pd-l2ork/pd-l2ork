"use strict";
var gui = require("nw.gui");
var pdgui = require("./pdgui.js");
var pd_menus = require("./pd_menus.js");

window.update_window_submenu = pd_menus.update_window_submenu;

// ico@vt.edu 2022-11-08: make this 1 if you wish to provide a 3-second delay
// before a newly opened patch window should proceed. dev tools will also
// automatically open to allow to use performance analysis tool to seek
// potential performance bottlenecks. leave this 0 to make it run normally.
var PD_L2ORK_NW_DEBUG = 0;

// We're using the following pwd variable as the default dir in which various
// file dialogs open, so we need to initialize it in some way. The following
// provides a reasonable default (use the PWD if it's available, otherwise
// fall back to HOME, or USERPROFILE on Windows). This will be updated later by
// the engine through pdgui.gui_set_cwd() once the engine has completed its
// startup.

// If you start Pd by clicking the app bundle in OSX, the PWD environment
// variable doesn't exist. In that case we use HOME instead.
var pwd = process.env.PWD !== undefined ? process.env.PWD : process.env.HOME;

// Windows doesn't have either of the environment variables above, so we
// use USERPROFILE because it also includes drive letter (HOMEPATH apparently
// doesn't), so, this can pollute patch_name and other code relying on
// canvasinfo, resulting in openpanel->(pdgui.js) file_dialog() not working.
if (!pwd) {
    pwd = process.env.USERPROFILE;
}

// gui preset
pdgui.skin.apply(window);

// For translations
var l = pdgui.get_local_string;

function have_args() {
    return !!gui.App.argv.length;
}

function set_vars(win) {
    var port_no, font_engine_sanity, pd_engine_id, argv_offset;
    // If the GUI was started by Pd, our port number is going to be
    // the first argument. If the GUI is supposed to start Pd, we won't
    // have any arguments and need to set it here.
    if (have_args() && gui.App.argv.length > 1) {
        // Unfortunately there's a bug in nw.js where the argument that
        // specifies the package.json path doesn't get included in the
        // argv array. This happens under Windows and Linux but apparently
        // not under OSX. That means we need an offset hack
        argv_offset = process.platform === "darwin" ? 1 : 0;
        port_no = gui.App.argv[1 + argv_offset]; // fed to us by the Pd process
        // address unique to the pd_engine
        pd_engine_id = gui.App.argv[5 + argv_offset];
    } else {
        // If we're starting Pd, this is the first port number to try. (We'll
        // increment it if that port happens to be taken.)
        port_no = 5400;
    }
    pdgui.set_port(port_no);
    pdgui.set_pd_engine_id(pd_engine_id);
    pdgui.set_pwd(pwd);
    pdgui.set_pd_window(win);
    font_engine_sanity = pdgui.set_font_engine_sanity(win);
    pdgui.set_app_quitfn(app_quit);
    pdgui.set_open_html_fn(open_html);
    pdgui.set_open_textfile_fn(open_textfile);
    pdgui.set_open_external_doc_fn(open_external_doc);

    // nw context callbacks (mostly just creating/destroying windows)
    pdgui.set_new_window_fn(nw_create_window);
    pdgui.set_close_window_fn(nw_close_window);
}

function app_quit() {
    pdgui.write_completion_index();
    console.log("quitting Pd...");
    gui.App.quit();
}

function open_html(target) {
    gui.Shell.openItem(target);
}

function open_textfile(target) {
    gui.Shell.openItem(target);
}

function open_external_doc(target) {
    gui.Shell.openExternal(target);
}

function nw_window_focus_callback() {
    if (process.platform === "darwin") {
        nw_create_pd_window_menus(gui, window);
    }
}

// This should be merged with the same function name in pd_canvas.js,
// except that we're not saving the Pd Window zoomlevel anywhere
function nw_window_zoom(delta) {
    var z = gui.Window.get().zoomLevel;
    z += delta;
    if (z < 8 && z > -8) {
        gui.Window.get().zoomLevel = z;
    }
}

function connect() {
    var gui_path, file_path;
    if (have_args() && gui.App.argv.length > 1) {
        // Pd started the GUI, so connect to it on port provided in our args
        pdgui.post("Pd has started the GUI");
        pdgui.connect_as_client();
    } else {
        // create a tcp server, then spawn Pd with "-guiport" flag and port
        gui_path = process.cwd();
        // On OSX, if the user double-clicks a pd file we get a single
        // argument prefixed with "file://". If so, we parse it and
        // feed it to the Pd instance as an argument to the "-open" flag
        file_path = gui.App.argv[0];
        if (have_args() && file_path.slice(0, 7) === "file://") {
            file_path = decodeURI(file_path.slice(7));
        }
        pdgui.post("GUI is starting Pd...");
        pdgui.connect_as_server(gui_path, file_path);
    }
}

function console_unwrap_tag(console_elem, tag_name) {
    var b = console_elem.getElementsByTagName(tag_name),
        parent_elem;
    while (b.length) {
        parent_elem = b[0].parentNode;
        while(b[0].firstChild) {
            parent_elem.insertBefore(b[0].firstChild, b[0]);
        }
        parent_elem.removeChild(b[0]);
        parent_elem.normalize();
    }
}

// We need to use a regular expression to search without regard to case
function escapeRegExp(string){
  // $& means the whole matched string
  return string.replace(/[.*+?^${}()|\[\]\\]/g, "\\$&");
}

function console_find_text(evt, callback) {
    var console_text = document.getElementById("p1"),
        wrap_tag = "mark",
        wrapper_count,
        elem = evt.target;
    window.setTimeout(function () {
        var find_text = new RegExp(escapeRegExp(elem.value), "gi");
        console_unwrap_tag(console_text, wrap_tag);
        // Check after the event if the value is empty
        if (elem.value === undefined || elem.value === "") {
            // Todo: use class instead of style here
            elem.style.setProperty("background", "white");
        } else {
            window.findAndReplaceDOMText(console_text, {
                //preset: "prose",
                find: find_text,
                wrap: wrap_tag
            });
            // The searchAndReplace API is so bad you can't even know how
            // many matches there were without traversing the DOM and
            // counting the wrappers!
            wrapper_count = console_text.getElementsByTagName(wrap_tag).length;
            // Todo: use class instead of style here...
            if (wrapper_count < 1) {
                elem.style.setProperty("background", "red");
            } else {
                elem.style.setProperty("background", "white");
            }
        }
        if (callback) {
            callback();
        }
    }, 0);
}

// start at top and highlight the first result after a search
function console_find_callback() {
    var highlight_checkbox = document.getElementById("console_find_highlight");
    console_find_highlight_all(highlight_checkbox);
    console_find_traverse.set_index(0);
    console_find_traverse.next();
}

function console_find_keypress(e) {
    console_find_text(e, console_find_callback);
}

function console_find_highlight_all(elem) {
    var matches,
        highlight_tag = "console_find_highlighted",
        state = elem.checked,
        i;
    matches = document.getElementById("p1")
        .getElementsByClassName(highlight_tag);
    // remember-- matches is a _live_ collection, not an array.
    // If you remove the highlight_tag from an element, it is
    // automatically removed from the collection. I cannot yet
    // see a single benefit to this behavior-- here, it means
    // we must decrement i to keep from skipping over every
    // other element... :(
    for (i = matches.length - 1; i >= 0; i--) {
        matches[i].classList.remove(highlight_tag);
    }
    if (state) {
        matches = document.getElementById("p1").getElementsByTagName("mark");
        for (i = 0; i < matches.length; i++) {
            matches[i].classList.add(highlight_tag);
        }
    }
}

var console_find_traverse = (function () {
    var count = 0,
        console_text = document.getElementById("p1"),
        wrap_tag = "mark",
        last_action = 0; // direction last request was made
    return {
        next: function () {
            count += last_action * (last_action >= 0 ? 1 : -1);
            var i, last, next,
                elements = console_text.getElementsByTagName(wrap_tag);
            if (elements.length > 0) {
                i = count % elements.length;
                elements[i].classList.add("console_find_current");
                if (elements.length > 1) {
                    last = i === 0 ? elements.length - 1 : i - 1;
                    next = (i + 1) % elements.length;
                    elements[last].classList.remove("console_find_current");
                    elements[next].classList.remove("console_find_current");
                }
                // adjust the scrollbar to make sure the element is visible,
                // but only if necessary.
                // I don't think this is available on all browsers...
                elements[i].scrollIntoViewIfNeeded();
                last_action = 1;
            }
        },
        prev: function () {
            count += last_action * (last_action <= 0 ? 1 : -1);
            var i, last, next,
                elements = console_text.getElementsByTagName(wrap_tag);
            if (elements.length > 0) {
                if (count < 0) {
                    count += elements.length;
                }
                i = count % elements.length;
                elements[i].classList.add("console_find_current");
                if (elements.length > 1) {
                    last = i === 0 ? elements.length - 1 : i - 1;
                    next = (i + 1) % elements.length;
                    elements[last].classList.remove("console_find_current");
                    elements[next].classList.remove("console_find_current");
                }
                // adjust the scrollbar to make sure the element is visible,
                // but only if necessary.
                // I don't think this is available on all browsers...
                elements[i].scrollIntoViewIfNeeded();
                last_action = -1;
            }
        },
        set_index: function(c) {
            count = c;
            last_action = 0;
        }
    };
}());

// ico@vt.edu 2020-10-30: Here we prevent autorepeat from messing with the
// shift key detection (LATER: may use the same approach for other modifiers)
var shiftKey = 0;
function console_find_keydown(evt) {
    //pdgui.post("keydown=" + evt.keyCode);
    if (evt.keyCode === 13) {
        if (evt.shiftKey && shiftKey === 0) {
            shiftKey = 1;
        }
        if (shiftKey) {
            console_find_traverse.prev();
        } else {
            console_find_traverse.next();
        }
        evt.stopPropagation();
        evt.preventDefault();
        return false;
    } else if (evt.keyCode === 27) { // escape

    } else if (evt.keyCode === 8 || // backspace or delete
               evt.keyCode === 46) {
        console_find_text(evt, console_find_callback);
    }
}

function console_find_keyup(evt) {
    //pdgui.post("keyup=" + evt.keyCode);
    if (!evt.shiftKey && shiftKey === 1) {
        shiftKey = 0;
    }
}

function add_events() {
    // Find bar
    var find_bar = document.getElementById("console_find_text");
    find_bar.placeholder = l("pd_window.find.placeholder");
    // Forward console key events to Pd using the global "pd key" method...
    document.addEventListener("keydown", function(evt) {
        pdgui.keydown("pd", evt);
    }, false);
    document.addEventListener("keypress", function(evt) {
        pdgui.keypress("pd", evt);
    }, false);
    document.addEventListener("keyup", function(evt) {
        pdgui.keyup("pd", evt);
    }, false);
    find_bar.addEventListener("keydown",
        function(e) {
            return console_find_keydown(e);
        }, false
    );
    find_bar.addEventListener("keyup",
        function(e) {
            return console_find_keyup(e);
        }, false
    );
    find_bar.addEventListener("keypress",
        function(e) {
            console_find_keypress(e);
        }, false
    );
    find_bar.addEventListener("cut",
        function(e) {
            console_find_text(e, console_find_callback);
        }, false
    );
    find_bar.addEventListener("paste",
        function(e) {
            console_find_text(e, console_find_callback);
        }, false
    );
    // DSP toggle
    document.getElementById("dsp_control").addEventListener("click",
        function(evt) {
            var dsp_state = evt.target.checked ? 1 : 0;
            pdgui.pdsend("pd dsp", dsp_state);
        }
    );
    // Opening another nwjs pd-l2ork instance
    nw.App.on("open", function(argv_string) {
        //pdgui.post("nw.App.on... " + argv_string);
        var port,
            host,
            pd_engine_id,
            argv;
        if (argv_string.slice(0, 7) === "file://") {
                // Clicking on a Pd file with an installed OSX app bundle sends
                // a single argument which is a file:// URI.
                // With the OSX app bundle it is the GUI which starts the
                // Pd process. So in this case, we just need to parse the
                // file and open it.
                // Selecting multiple files and clicking "Open" will trigger
                // a separate "open" event for each file, so luckily we don't
                // have to parse them.
                pdgui.menu_open(decodeURI(argv_string.slice(7)));
        } else {
                if (argv_string.includes("pd-l2ork-k12")) {
                    // since this is calling a separate instance, only
                    // command line option would trigger K12 mode, yet
                    // we have arrived here because of the second instance
                    // calling opening of new files without the unique flag.
                    // if so, we should really keep the first instance
                    // in whatever mode it was already in. therefore, here
                    // we only reformat pd-l2ork-k12, so that it does not
                    // cause problems during parsing below.
                    //pdgui.post("...second instance has k12 mode enabled");
                    //pdgui.k12_mode = 1;
                    argv_string = argv_string.replace("pd-l2ork-k12", "pd-l2ork");
                }
                if (pdgui.nw_os_is_windows) {
                    //pdgui.post("...we are running on windows...");
                    // Otherwise we assume that the Pd process tried to
                    // open the GUI, supplying us with a port number and
                    // an instance id. On windows we try to harvest files
                    // directly off the arguments. They are found at the
                    // end, following the openpatches argument, which serves
                    // as a point of delineation.
                    // CURRENTLY we are still relying on the old approach,
                    // which is rather messy, so we may revisit this later
                    //var win_args = new Array();
                    var win_args = argv_string.split(" pd-l2ork ");

                    // Windows refuses to connect to pd-l2ork engine unless
                    // you indicate IP of 0.0.0.0. (localhost or 127.0.0.1
                    // results in connection refused inside the
                    // connect_as_client_to_secondary_instance function)
                    // ico@vt.edu 2025-07-19: it seems 127.0.0.1 now also works
                    //host = win_args[0].split(" ").slice(-1).join();
                    //pdgui.post("win_args[0]=" + win_args[0] + "\n[1]=" + win_args[1]);
                    host = "0.0.0.0";
                    port = win_args[0].split(" ").slice(-2, -1).join();
                    // ico@vt.edu 2025-07-19: the following stopped working
                    // on windows only, so when we test for different instances
                    // we for the time being ignore on windows comparison between
                    // primary and secondary instances (see pdgui.js'
                    // gui_open_via_unique function
                    pd_engine_id = win_args[1].split(" ").pop();
                    // the following gets us the port, but that does not work
                    // as a comparison for the unique instance since the instance
                    // is also sent from c (see s_main.c)
                    //pd_engine_id = win_args[0].split(" ").slice(-2,-1);
                    //pdgui.post("...argv_string final: host=" + host + " port=" + port + " engine=" + pd_engine_id);

                    // LATER: consider taking this approach instead,
                    // as it may end-up being a lot cleaner
                    //win_args = argv_string.split("openpatches");
                    //pdgui.menu_open(win_args[1].trim());

                    pdgui.connect_as_client_to_secondary_instance(host, port,
                        pd_engine_id);
                } else {
                    // Otherwise we assume that the Pd process tried to
                    // open the GUI, supplying us with a port number and
                    // an instance id. In this case, we need to create a
                    // socket connection and fetch the file-list...
                    pd_engine_id = argv_string.split(" ").slice(-1).join();
                    argv_string = argv_string.slice(0, -pd_engine_id.length).trim();
                    // strip off the gui dir
                    argv_string = argv_string.slice(0,
                        -nw.App.argv[4].length).trim();
                    if (process.platform === "win32") {
                        // windows quotes this string, so let's remove the two
                        // quotation marks
                        argv_string = argv_string.slice(0, -2).trim();
                    }
                    // now strip off the k12 string, which is guaranteed not
                    // to have any spaces in it
                    argv_string = argv_string.slice(0,
                            -argv_string.split(" ").slice(-1).join().length).trim();
                    // now get the host string and the port
                    host = argv_string.split(" ").slice(-1).join();
                    port = +argv_string.split(" ").slice(-2, -1).join();
                    //pdgui.post("...argv_string final: host=" +
                    //    host + " port=" + port + " engine=" + pd_engine_id);
                    pdgui.connect_as_client_to_secondary_instance(host, port,
                        pd_engine_id);
                }
        }
    });
    // Browser Window Close
    gui.Window.get().on("close", function () {
        pdgui.menu_quit();
    });
    // Focus callback for OSX
    gui.Window.get().on("focus", function () {
        //pdgui.post("focus: console");
        nw_window_focus_callback();
    });
    // Pd Window zooming with mousewheel
    document.addEventListener("wheel", function(evt) {
        // ico@vt.edu 2022-12-16: prevent unwanted
        // zoom due to intertial wheel behavior.
        var threshold = 20 + (process.platform === "darwin" ? 280 : 0);
        if (evt.deltaY > -threshold && evt.deltaY < threshold && 
            evt.deltaX > -threshold && evt.deltaX < threshold) {
            //pdgui.post("minimal delta");
            return;
        }
        //pdgui.post("console wheel " + evt.deltaY);
        if (pdgui.cmd_or_ctrl_key(evt)) {
            if (evt.deltaY < 0) {
                nw_window_zoom(+1);
            } else if (evt.deltaY > 0) {
                nw_window_zoom(-1);
            }
        }
    }, false);
    // Open dialog
    document.getElementById("fileDialog").setAttribute("nwworkingdir", pwd);
    document.getElementById("fileDialog").setAttribute("accept",
        Object.keys(pdgui.pd_filetypes).toString());

    // [openpanel] and [savepanel] callbacks
    document.querySelector("#openpanel_dialog").addEventListener("change",
        function(evt) {
            var file_string = evt.target.value;
            // reset value so that we can open the same file twice
            evt.target.value = null;
            pdgui.file_dialog_callback(file_string);
            console.log("tried to openpanel something");
        }, false
    );
    document.querySelector("#savepanel_dialog").addEventListener("change",
        function(evt) {
            var file_string = evt.target.value;
            // reset value so that we can open the same file twice
            evt.target.value = null;
            pdgui.file_dialog_callback(file_string);
            console.log("tried to savepanel something");
        }, false
    );

    // disable drag and drop for the time being (TODO!)
    window.addEventListener("dragover", function (evt) {
        evt.preventDefault();
    }, false);
    window.addEventListener("drop", function (evt) {
        var num_files = evt.dataTransfer.files.length;
        var i, file_array = "";
        for (i = 0; i < num_files; i++) {
            // LATER: rethink how files are separated since
            // semicolon can be embedded inside a filename
            file_array += evt.dataTransfer.files[i].path + ";";
        }
        pdgui.menu_open(file_array);
        evt.preventDefault();
    }, false);
}

function nw_close_window(window) {
    window.close(true);
}

// 0.46+ seems to be required for "null" to work. TODO: Bisect to get the
// actual minimum required version for this.
var null_pos = pdgui.check_nw_version("0.46") ? "null" : "center";

function pd_l2ork_debug_sleep(milliseconds) {
  const date = Date.now();
  let currentDate = null;
  do {
    currentDate = Date.now();
  } while (currentDate - date < milliseconds);
}

function nw_create_window(cid, type, width, height, xpos, ypos, attr_array) {
    // todo: make a separate way to format the title for OSX
    var my_title;
    var win_frame = true;
    var win_transparent = false;
    if (type === "pd_canvas") {
        my_title = pdgui.format_window_title(
            attr_array.name,
            attr_array.dirty,
            attr_array.args,
            attr_array.dir);

	    // ico@vt.edu 2020-08-13:
	    // why does Windows have different innerWidth and innerHeight from other OSs?
	    // See pdgui.js' canvas_params for the explanation...
	    // ico@vt.edu 2020-08-21: this should only apply to patch windows
        // 2020-10-01: this is not needed anymore since it was a bug specific to 0.14.7
        // and Windows is now using 0.24.4
	    //width -= 16 * pdgui.nw_os_is_windows;
	    //height -= 8 * pdgui.nw_os_is_windows;
    } else {
        my_title = type;
        if (type !== "search" && type !== "text") {
            win_frame = false;
            win_transparent = true;
        }
    }
    var my_file =
        type === "pd_canvas" ? "pd_canvas.html" : "dialog_" + type + ".html";

    var eval_string = "register_window_id(" +
                      JSON.stringify(cid) + ", " +
                      JSON.stringify(attr_array) + ");";
    //pdgui.post("x=" + xpos + " y=" + ypos);
    var pos;
    if (xpos == 0 && ypos == 0) {
        pos = "center";
    } else {
        pos = null_pos;
        //pdgui.post("check_os=" + pdgui.check_os("win32"));
        if (pdgui.nw_os_is_linux == 1) {
            //pdgui.post("offset=" + pdgui.get_nw_menu_offset());
            ypos = ypos - pdgui.get_nw_menu_offset();
        }
    }

    //pdgui.post("nw_create_window w=" + width + " h=" + height);

    /*
    // ico@vt.edu: instantiate default options for the window behavior
    // the first two vars (transparent is currently disabled) are options
    // to be passed to the window at creation time, while second two are
    // activated via a function call after the window has been created
    var frame_val = true,
        //transparent_val = true,
        resize_val = true,
        topmost_val = false,
        menu_val = true;

    if (frame_option !== undefined) {
        pdgui.post("window_options have content");
        process_window_options(window_options,
            frame_val, transparent_val,
            resize_val, topmost_val);
        // TODO: do stuff here
    }*/
    gui.Window.open(my_file, {
        title: my_title,
        // ico@vt.edu: position in 0.46.2 overrides x and y below
        //position: pos,
        focus: true,
        width: width,
        // add to the height to address OS idiosyncrasies and accommodate
        // the menu height (LATER: see if we need to compensate here if we
        // don't have menu visible, that certainly is not an issue for OSX)
        height: height
                // 1. nw_menu_offset value on non-OSX OSs
                + (pdgui.get_nw_menu_offset() * !pdgui.nw_os_is_osx)
                // 2. OSX window size offset to match that of Windows
                - (4 * pdgui.nw_os_is_osx)
                // 3. default Raspbian DM offset to ensure same window height
                //    as Windows
                + (pdgui.get_nw_menu_offset() === 0 ? 20 : 0)
                // 4. default Ubuntu Linux window size offset that is negated
                //    by the Raspberry Pi's DM that has no nw_menu_offset, in
                //    which case this is multiplied by 0
                - (16 * pdgui.nw_os_is_linux * (pdgui.get_nw_menu_offset() > 0 ? 1 : 0)),
        x: Math.floor(xpos),
        y: Math.floor(ypos),
        frame: win_frame,
        transparent: win_transparent
    }, function (new_win) {

        // ico@vt.edu 2022-11-06: let's open dev tools and give time to
        // start recording data before we start loading the patch
        if (PD_L2ORK_NW_DEBUG === 1)
        {
            new_win.showDevTools();
            pd_l2ork_debug_sleep(3000);
        }

        if (type === "pd_canvas") {
            pdgui.set_patchwin(cid, new_win);
        } else {
            pdgui.set_dialogwin(cid, new_win);
        }
        new_win.on("loaded", function() {
            // Off to the races... :(
            // We need to check here if we're still the window pointed to
            // by the GUI's array of toplevel windows. It can easily happen
            // that we're not-- for example the user could send a stream
            // of [vis 1, vis 0, vis 1, etc.( to a single subpatch. In that
            // case the asynchronous Pd <-> GUI communication might
            // momentarily create multiple windows of that same subpatch.
            // Here we just let them load, then close any that don't match
            // the cid we added above.
            // Additionally, we check to make sure that the cid is registered
            // as a loaded canvas. If not, we assume it got closed before
            // we were able to finish loading the browser window (e.g.,
            // with a [vis 1, vis 0( message). In that case we kill the window.

            // Still, this is pretty fortuitous-- we have two levels of
            // asynchronicity-- creating the nw window and loading it. There
            // may still be an edge case where a race between those two causes
            // unpredictable behavior.
            if ((new_win === pdgui.get_patchwin(cid) ||
                 new_win === pdgui.get_dialogwin(cid))
                && pdgui.window_is_loading(cid)) {
                // initialize the window
                new_win.eval(null, eval_string);
                // flag the window as loaded. We may want to wait until the
                // DOM window has finished loading for this.
                //pdgui.post("window finished loading...");
                pdgui.set_window_finished_loading(cid, new_win);
            } else {
                // If the window is no longer loading, we need to go ahead
                // and remove the reference to it in the patchwin array.
                // Otherwise we get dangling references to closed windows
                // and other bugs...
                if (!pdgui.window_is_loading(cid)) {
                    if (type === "pd_canvas") {
                        pdgui.set_patchwin(cid, undefined);
                    } else {
                        pdgui.set_dialogwin(cid, undefined);
                    }
                }
                new_win.close(true);
            }
        });
    });
}

// Pd Window Menu Bar

function minit(menu_item, options) {
    var key;
    for (key in options) {
        if (options.hasOwnProperty(key)) {
            menu_item[key] = options[key];
        }
    }
}

// used, so that we can reference menu later
var pd_m  = null;

function nw_create_pd_window_menus(gui, w) {
    //pdgui.post("nw_create_pd_window_menus");
    if (process.platform !== "darwin" || pd_m === null) {
        //pdgui.post("nw_create_pd_window_menus first time");
        pd_m = pd_menus.create_menu(gui, "console");
        /*
        var item = new nw.MenuItem({
            label: "testing...",
            tooltip: "testing..."
        });
        pd_m.win.win_list.submenu.append(item);
        */
    }

    // On OSX we have menu items for canvas operations-- we need to disable
    // them when the console gets focus.
    var osx = process.platform === "darwin";

    // File sub-entries
    minit(pd_m.file.new_file, { click: pdgui.menu_new });
    minit(pd_m.file.open, {
        click: function (){
            var input, chooser,
                span = w.document.querySelector("#fileDialogSpan");
            // Complicated workaround-- see comment in build_file_dialog_string
            input = pdgui.build_file_dialog_string({
                style: "display: none;",
                type: "file",
                id: "fileDialog",
                nwworkingdir: pdgui.get_pd_opendir(),
                multiple: null,
                // These are copied from pd_filetypes in pdgui.js
                accept: ".pd,.pat,.mxt,.mxb,.help"
            });
            span.innerHTML = input;
            chooser = w.document.querySelector("#fileDialog");
            // Hack-- we have to set the event listener here because we
            // changed out the innerHTML above
            chooser.onchange = function() {
                var file_array = chooser.value;
                // reset value so that we can open the same file twice
                chooser.value = null;
                pdgui.menu_open(file_array);
                console.log("tried to open something");
            };
            chooser.click();
        }
    });

    // Note: this must be different for the main Pd window
    if (osx) {
        minit(pd_m.file.save, { enabled: false });
        minit(pd_m.file.saveas, { enabled: false });
    }

    minit(pd_m.file.k12_demos, {
        click: function () {
            //pdgui.post("os_is_win=" + pdgui.nw_os_is_windows + " orig_path=<" +
            //    pdgui.lib_dir + "/extra/K12/demos" + " funkyfied_path=<" +
            //    pdgui.funkify_windows_path(pdgui.get_lib_dir() +
            //    "/extra/K12/demos") + ">");
            var input, chooser,
                span = w.document.querySelector("#fileDialogSpan");
            // Complicated workaround-- see comment in build_file_dialog_string
            input = pdgui.build_file_dialog_string({
                style: "display: none;",
                type: "file",
                id: "fileDialog",
              nwworkingdir: (pdgui.nw_os_is_windows ? 
                  pdgui.funkify_windows_path(pdgui.get_lib_dir() +
                      "/extra/K12/demos") :
                  (pdgui.get_lib_dir() + "/extra/K12/demos")),
                multiple: null,
                // These are copied from pd_filetypes in pdgui.js
                accept: ".pd,.pat,.mxt,.mxb,.help"
            });
            span.innerHTML = input;
            chooser = w.document.querySelector("#fileDialog");
            // Hack-- we have to set the event listener here because we
            // changed out the innerHTML above
            chooser.onchange = function() {
                var file_array = chooser.value;
                // reset value so that we can open the same file twice
                chooser.value = null;
                pdgui.menu_open(file_array);
                console.log("tried to open something");
            };
            chooser.click();
        }
    });

    minit(pd_m.file.k12_mode, { click: set_k12_mode });

    minit(pd_m.file.message, { click: pdgui.menu_send });
    if (osx) {
        minit(pd_m.file.close, { enabled: false });
    }
    if (!osx) {
        minit(pd_m.file.quit, { click: pdgui.menu_quit });
    }

    // Edit sub-entries
    if (osx) {
        minit(pd_m.edit.undo, { enabled: false });
        minit(pd_m.edit.redo, { enabled: false });
        minit(pd_m.edit.cut, { enabled: false });
    }
    minit(pd_m.edit.copy, { enabled: true,
        click: function() {
            w.document.execCommand("copy");
        }
    });
    if (osx) {
        minit(pd_m.edit.paste, { enabled: false });
        minit(pd_m.edit.paste_clipboard, { enabled: false });
        minit(pd_m.edit.duplicate, { enabled: false });
    }
    minit(pd_m.edit.selectall, {
        enabled: true,
        click: function () {
            var container_id = "p1", range;
            // ico@vt.edu 2020-10-30 if we are doing select all inside find box
            var find_bar = w.document.getElementById("console_find"),
                state = find_bar.style.getPropertyValue("display");
            if (state !== "none") {
                document.execCommand("selectAll");
            } else if (w.document.selection) {
                // This should work across browsers
                range = w.document.body.createTextRange();
                range.moveToElementText(w.document.getElementById(container_id));
                range.select();
            } else if (w.getSelection) {
                range = w.document.createRange();
                range.selectNode(w.document.getElementById(container_id));
                // we need to empty the current selection to avoid a strange
                // error when trying to select all right after Pd starts:
                // "The given range and the current selection belong to two
                //  different document fragments."
                // (I guess nw.js somehow starts up with the selection being
                // somewhere outside the window...)
                w.getSelection().empty();
                w.getSelection().addRange(range);
            }
        }
    });
    minit(pd_m.edit.clear_console, {
        enabled: true,
        click: pdgui.clear_console
    });
    if (osx) {
        minit(pd_m.edit.reselect, { enabled: false });
    }
    if (osx) {
        minit(pd_m.edit.encapsulate, { enabled: false });
        minit(pd_m.edit.tidyup, { enabled: false });
        minit(pd_m.edit.font, { enabled: false });
        minit(pd_m.edit.cordinspector, { enabled: false });
    }
    minit(pd_m.edit.find, {
        click: function () {
            var find_bar = w.document.getElementById("console_find"),
                find_bar_text = w.document.getElementById("console_find_text"),
                text_container = w.document.getElementById("console_bottom"),
                state = find_bar.style.getPropertyValue("display");
            if (state === "none") {
                text_container.style.setProperty("bottom", "1.6em");
                find_bar.style.setProperty("display", "inline");
                //find_bar.style.setProperty("height", "1.2em");
                // Don't do the following in logical time so that the
                // console_find keypress event won't receive this shortcut key
                window.setTimeout(function() {
                    find_bar_text.focus();
                    find_bar_text.select();
                }, 0);
            } else {
                // Blur focus so that the console_find keypress doesn't
                // receive our shortcut key
                find_bar.blur();
                text_container.style.setProperty("bottom", "0px");
                find_bar.style.setProperty("display", "none");
            }
        }
    });
    if (osx) {
        minit(pd_m.edit.findagain, { enabled: false });
        minit(pd_m.edit.finderror, { enabled: false });
        minit(pd_m.edit.autotips, { enabled: false });
        minit(pd_m.edit.editmode, { enabled: false });
    }
    minit(pd_m.edit.preferences, {
        click: pdgui.open_prefs
    });

    // View menu
    minit(pd_m.view.zoomin, {
        click: function () {
            nw_window_zoom(+1);
        }
    });
    minit(pd_m.view.zoomout, {
        click: function () {
            nw_window_zoom(-1);
        }
    });
    minit(pd_m.view.zoomreset, {
        click: function () {
            gui.Window.get().zoomLevel = 0;
        }
    });
    minit(pd_m.view.fullscreen, {
        click: function() {
            var win = gui.Window.get();
            win.toggleFullscreen();
        }
    });

    // Put menu
    if (osx) {
        minit(pd_m.put.object, { enabled: false });
        minit(pd_m.put.message, { enabled: false });
        minit(pd_m.put.number, { enabled: false });
        minit(pd_m.put.symbol, { enabled: false });
        minit(pd_m.put.list, { enabled: false });
        minit(pd_m.put.comment, { enabled: false });
        minit(pd_m.put.dropdown, { enabled: false });
        minit(pd_m.put.bang, { enabled: false });
        minit(pd_m.put.toggle, { enabled: false });
        minit(pd_m.put.number2, { enabled: false });
        minit(pd_m.put.vslider, { enabled: false });
        minit(pd_m.put.hslider, { enabled: false });
        minit(pd_m.put.knob, { enabled: false });
        minit(pd_m.put.vradio, { enabled: false });
        minit(pd_m.put.hradio, { enabled: false });
        minit(pd_m.put.vu, { enabled: false });
        minit(pd_m.put.cnv, { enabled: false });
        minit(pd_m.put.image, { enabled: false });
        //minit(pd_m.put.graph, { enabled: false });
        minit(pd_m.put.array, { enabled: false });
        minit(pd_m.put.k12_menu, { enabled: false });
    }

    // Winman sub-entries
    
    minit(pd_m.win.nextwin, {
        click: function() {
            pdgui.raise_next("pd_window");
        }
    });
    minit(pd_m.win.prevwin, {
        click: function() {
            pdgui.raise_prev("pd_window");
        }
    });
    
    if (osx) {
        minit(pd_m.win.parentwin, { enabled: false });
        minit(pd_m.win.visible_ancestor, { enabled: false });
        minit(pd_m.win.pdwin, { enabled: false });
    }

    // Media sub-entries
    minit(pd_m.media.audio_on, {
        click: function() {
            pdgui.pdsend("pd dsp 1");
        }
    });
    minit(pd_m.media.audio_off, {
        click: function() {
            pdgui.pdsend("pd dsp 0");
        }
    });
    minit(pd_m.media.test, {
        click: function() {
            pdgui.pd_doc_open("doc/7.stuff/tools", "testtone.pd", 1);
        }
    });
    minit(pd_m.media.loadmeter, {
        click: function() {
            pdgui.pd_doc_open("doc/7.stuff/tools", "load-meter.pd", 1);
        }
    });

    // Help sub-entries
    minit(pd_m.help.about, {
        click: function() {
            pdgui.pd_doc_open("doc/about", "about.pd", 1);
        }
    });
    minit(pd_m.help.manual, {
        click: function() {
            pdgui.pd_doc_open("doc/1.manual", "index.htm");
        }
    });
    minit(pd_m.help.browser, {
        click: pdgui.open_search
    });
    minit(pd_m.help.intro, {
        click: function() {
            pdgui.pd_doc_open("doc/5.reference", "help-intro.pd", 1);
        }
    });
    minit(pd_m.help.l2ork_list, {
        click: function() {
            pdgui.external_doc_open("http://disis.music.vt.edu/listinfo/l2ork-dev");
        }
    });
    minit(pd_m.help.pd_list, {
        click: function() {
            pdgui.external_doc_open("http://puredata.info/community/lists");
        }
    });
    minit(pd_m.help.forums, {
        click: function() {
            pdgui.external_doc_open("http://forum.pdpatchrepo.info/");
        }
    });
    minit(pd_m.help.irc, {
        click: function() {
            pdgui.external_doc_open("http://puredata.info/community/IRC");
        }
    });
    minit(pd_m.help.devtools, {
        click: function () {
            gui.Window.get().showDevTools();
        }
    });
}

function post_startup_messages() {
    // These will be the first messages printed to the main Pd window.
    // Later let's use a link to the docs for new users.
    pdgui.post("Welcome to Pd-L2Ork");
    // Report the nw.js version that we're using.
    pdgui.post("nw.js version " + process.versions["nw"]);
    // Warn the user if the font sizes aren't optimal. Font sizes which
    // aren't optimal result in extra space at the end of object/message
    // boxes
    if (!pdgui.get_font_engine_sanity()) {
        pdgui.post("warning: your system's font stack is not optimal");
    }
}

var linux_titlebar_offset_kludge = false;

// this is invoked only by the main pd window
function gui_init(win) {
    set_vars(win);
    add_events();
    nw_create_pd_window_menus(gui, win);
    // Set up the Pd Window
    gui.Window.get().setMinimumSize(350, 250);
    post_startup_messages();
    // Now we create a connection from the GUI to Pd, in one of two ways:
    // 1) If the GUI was started by Pd, then we create a tcp client and
    //    connect on the port Pd fed us in our command line arguments.
    // 2) If Pd hasn't started yet, then the GUI creates a tcp server and spawns
    //    Pd using the "-guiport" flag with the port the GUI is listening on.
    // Pd always starts the GUI with a certain set of command line arguments. If
    // those arguments aren't present then we assume we need to start Pd.
    connect();
    pdgui.restore_apps(0);
    if (pdgui.nw_os_is_linux) {
        // set pdgui.js' nw_menu_offset to 0 since we are dealing with Linux.
        // this is because if the callback below is called (which means offset
        // is other than 0), it will re-set the offset to the appropriate size.
        // we do this because on RPi which uses nwjs 0.60.1, move call is not
        // called unless the values have changed, so we never reach the "on move"
        // part. thus we set it here to 0, which works for RPi, while other
        // Linux distros (or at least Ubuntu) that use nwjs 0.67.1 will call
        // "on move" even if the values have not changed. if we ever get RPi's
        // version of nwjs to match that of other Linux (intel) platforms,
        // this part can be removed.
        pdgui.set_nw_menu_offset(0);
        //pdgui.post("window position " + gui.Window.get().x + " " + gui.Window.get().y);
        var pre_titlebar_y = gui.Window.get().y;
        gui.Window.get().moveTo(gui.Window.get().x, gui.Window.get().y);
        // ico@vt.edu 2022-11-19: this for some reason does not work with
        // the addEventListener approach so, we need to use the kludge global variable
        gui.Window.get().on("move", function() {
            if (!linux_titlebar_offset_kludge) {
                //pdgui.post("titlebar offset=" + (pre_titlebar_y - gui.Window.get().y));
                gui.Window.get().moveBy(0, (pre_titlebar_y - gui.Window.get().y) * 2);
                pdgui.set_nw_menu_offset(gui.Window.get().y - pre_titlebar_y);
                //pdgui.post("window offset = "  + pdgui.get_nw_menu_offset());
                linux_titlebar_offset_kludge = true;
            }
        });
    }
}

window.onload = function() {
    gui_init(window);       
};

function set_k12_mode() {
    pdgui.set_k12_mode(Number(pd_m.file.k12_mode.checked));
}

var k12_demos_item = new gui.MenuItem({
    label: l("menu.k12_demos"),
    tooltip: l("menu.k12_demos_tt"),
    click: pdgui.menu_k12_open_demos
});

function set_k12_mode_checkbox() {
    pd_m.file.k12_mode.checked = pdgui.get_k12_mode();
    /*
    this for some reason does not work
    if (pdgui.get_k12_mode()) {
        pd_m.file.insert(k12_demos_item, 4);
    } else {
        pd_m.file.remove(k12_demos_item);
    }
    */
}
