"use strict";

var l = pdbundle.pdgui.get_local_string; // For menu names

function menu_options(type, w, cid){
    var file_base = {
        // File section
        "menu-file" : {label: l("menu.file")},

        "file-message": {
            label: l("menu.message"),
            key: pdbundle.shortcuts.menu.message.key,
            modifiers: pdbundle.shortcuts.menu.message.modifiers,
            tooltip: l("menu.message_tt"),
            bottom_hr: {}
        },
    }
    
    var edit_base = {

        // Edit section
        "menu-edit": {label: l("menu.edit")},

        // Edit entries
        "edit-copy": {
            label: l("menu.copy"),
            key: pdbundle.shortcuts.menu.copy.key,
            modifiers: pdbundle.shortcuts.menu.copy.modifiers,
            tooltip: l("menu.copy_tt")
        },
        "edit-selectall": {
            label: l("menu.selectall"),
            key: pdbundle.shortcuts.menu.selectall.key,
            modifiers: pdbundle.shortcuts.menu.selectall.modifiers,
            tooltip: l("menu.selectall_tt")
        },
        "edit-find": {
            label: l("menu.find"),
            key: pdbundle.shortcuts.menu.find.key,
            modifiers: pdbundle.shortcuts.menu.find.modifiers,
            tooltip: l("menu.find_tt")
        },

    }

    var view_base = {
        // View section
        "menu-view": {label: l("menu.view")},

        // View entries
        "view-zoomin": {
            label: l("menu.zoomin"),
            key: pdbundle.shortcuts.menu.zoomin.key,
            modifiers: pdbundle.shortcuts.menu.zoomin.modifiers,
            tooltip: l("menu.zoomin_tt")
        },
        "view-zoomout": {
            label: l("menu.zoomout"),
            key: pdbundle.shortcuts.menu.zoomout.key,
            modifiers: pdbundle.shortcuts.menu.zoomout.modifiers,
            tooltip: l("menu.zoomout_tt")
        },
        "view-zoomreset": {
            label: l("menu.zoomreset"),
            key: pdbundle.shortcuts.menu.zoomreset.key,
            modifiers: pdbundle.shortcuts.menu.zoomreset.modifiers,
            tooltip: l("menu.zoomreset_tt"),
            top_hr: {}
        },
        "view-fullscreen": {
            label: l("menu.fullscreen"),
            key: pdbundle.shortcuts.menu.fullscreen.key,
            modifiers: pdbundle.shortcuts.menu.fullscreen.modifiers,
            tooltip: l("menu.fullscreen_tt"),
            top_hr: {}
        }
    }

    var window_base = {
        // Window section 
        "menu-window" : {label: l("menu.windows"),},

        // Window entries
        "window-nextwin" : {
            action: {onclick: function(){pdbundle.pdgui.raise_next("pd_window")}},
            label: l("menu.nextwin"),
            key: pdbundle.shortcuts.menu.nextwin.key,
            modifiers: pdbundle.shortcuts.menu.nextwin.modifiers,
            tooltip: l("menu.nextwin_tt")
        },
        "window-prevwin" : {
            action: {onclick: function(){pdbundle.pdgui.raise_prev("pd_window")}},
            label: l("menu.prevwin"),
            key: pdbundle.shortcuts.menu.prevwin.key,
            modifiers: pdbundle.shortcuts.menu.prevwin.modifiers,
            tooltip: l("menu.prevwin_tt")
        }
    }

    var console_menu_base = {
        // File
        ...file_base,

        // File entries
        "file-new" : {
            action: {onclick: pdbundle.pdgui.menu_new},
            label: l("menu.new"),
            key: pdbundle.shortcuts.menu.new.key,
            modifiers: pdbundle.shortcuts.menu.new.modifiers,
            tooltip: l("menu.new_tt")
        },

        "file-open": {
            label: l("menu.open"),
            key: pdbundle.shortcuts.menu.open.key,
            modifiers: pdbundle.shortcuts.menu.open.modifiers,
            tooltip: l("menu.open_tt")
        },

        // Edit
        ...edit_base,
        // Edit entries
        "edit-clear-console": {
            action: {onclick: pdbundle.pdgui.clear_console},
            label: l("menu.clear_console"),
            tooltip: l("menu.clear_console"),
            key: pdbundle.shortcuts.menu.clear_console.key,
            modifiers: pdbundle.shortcuts.menu.clear_console.modifiers,
            top_hr: {},
            bottom_hr: {}
        },
        "edit-preferences": {
            action: {onclick: pdbundle.pdgui.open_prefs},
            label: l("menu.preferences"),
            key: pdbundle.shortcuts.menu.preferences.key,
            modifiers: pdbundle.shortcuts.menu.preferences.modifiers,
            tooltip: l("menu.preferences_tt")
        },

        // View
        ...view_base,

        // Media section
        "menu-media" : {label: l("menu.media")},

        // Media entries
        "media-audio-on" : {
            action: {onclick: function(){pdbundle.pdgui.pdsend("pd dsp 1")}},
            label: l("menu.audio_on"),
            key: pdbundle.shortcuts.menu.audio_on.key,
            modifiers: pdbundle.shortcuts.menu.audio_on.modifiers,
            tooltip: l("menu.audio_on_tt")
        },
        "media-audio-off" : {
            action: {onclick: function(){pdbundle.pdgui.pdsend("pd dsp 0")}},
            label: l("menu.audio_off"),
            key: pdbundle.shortcuts.menu.audio_off.key,
            modifiers: pdbundle.shortcuts.menu.audio_off.modifiers,
            tooltip: l("menu.audio_off_tt"),
            bottom_hr: {}
        },
        "media-test" : {
            action: {onclick: function(){pdbundle.pdgui.pd_doc_open("doc/7.stuff/tools", "testtone.pd")}},
            label: l("menu.test"),
            tooltip: l("menu.test_tt")
        },
        "media-loadmeter" : {
            action: {onclick: function(){pdbundle.pdgui.pd_doc_open("doc/7.stuff/tools", "load-meter.pd")}},
            label: l("menu.loadmeter"),
            tooltip: l("menu.loadmeter_tt")
        },

        // Window
        ...window_base,

        // Help section
        "menu-help" : {label: l("menu.help")},

        // Help entries
        "help-about" : {
            action: {onclick: function(){pdbundle.pdgui.web_pd_doc_open("doc/about", "about.pd")}},
            label: l("menu.about"),
            tooltip: l("menu.about_tt")
        },
        "help-manual": {
            action: {onclick: function(){pdbundle.pdgui.pd_doc_open("doc/1.manual", "index.htm")}},
            label: l("menu.manual"),
            tooltip: l("menu.manual_tt")
        },
        "help-browser" : {
            action: {onclick: pdbundle.pdgui.open_search},
            label: l("menu.browser"),
            key: pdbundle.shortcuts.menu.browser.key,
            modifiers: pdbundle.shortcuts.menu.browser.modifiers,
            tooltip: l("menu.browser_tt")
        },
        "help-intro" : {
            action: {onclick: function(){pdbundle.pdgui.pd_doc_open("doc/5.reference", "help-intro.pd")}},
            label: l("menu.intro"),
            tooltip: l("menu.intro_tt"),
            bottom_hr: {}
        },
        "help-l2ork-list" : {
            action: {onclick: function(){pdbundle.pdgui.external_doc_open("http://disis.music.vt.edu/listinfo/l2ork-dev")}},
            label: l("menu.l2ork_list"),
            tooltip: l("menu.l2ork_list_tt")
        },
        "help-pd-list" : {
            action: {onclick: function(){pdbundle.pdgui.external_doc_open("http://puredata.info/community/lists")}},
            label: l("menu.pd_list"),
            tooltip: l("menu.pd_list_tt")
        },
        "help-forums" : {
            action: {onclick: function(){pdbundle.pdgui.external_doc_open("http://forum.pdpatchrepo.info/")}},
            label: l("menu.forums"),
            tooltip: l("menu.forums_tt")
        },
        "help-irc" : {
            action: {onclick: function(){pdbundle.pdgui.external_doc_open("http://puredata.info/community/IRC")}},
            label: l("menu.irc"),
            tooltip: l("menu.irc_tt")
        },
    }

    var console_menu = {
        ...console_menu_base,

        "file-recent-files": {
            recent_files: w.document.getElementById("file-recent-files"), //pass the recent files element holder
            label: l("menu.recent_files"),
            tooltip: l("menu.recent_files_tt"),
            bottom_hr: {}
        },

        "file-quit": {
            action: {onclick: pdbundle.pdgui.menu_quit},
            label: l("menu.quit"),
            key: pdbundle.shortcuts.menu.quit.key,
            modifiers: pdbundle.shortcuts.menu.quit.modifiers 
        },

        // Help entries
        "help-devtools" : {
            label: l("menu.devtools"),
            tooltip: l("menu.devtools_tt")
        }

    }

    var canvas_menu_base = {
        // File section
        ...file_base,
        // File entries
        "file-save": {
            label: l("menu.save"),
            key: pdbundle.shortcuts.menu.save.key,
            modifiers: pdbundle.shortcuts.menu.save.modifiers,
            tooltip: l("menu.save_tt")
        },
        "file-saveas": {
            label: l("menu.saveas"),
            key: pdbundle.shortcuts.menu.saveas.key,
            modifiers: pdbundle.shortcuts.menu.saveas.modifiers,
            tooltip: l("menu.saveas_tt")
        },
        "file-print": {
            label: l("menu.print"),
            key: pdbundle.shortcuts.menu.print.key,
            modifiers: pdbundle.shortcuts.menu.print.modifiers,
            tooltip: l("menu.print_tt"),
            bottom_hr: {}
        },
        "file-close": {
            label: l("menu.close"),
            key: pdbundle.shortcuts.menu.close.key,
            modifiers: pdbundle.shortcuts.menu.close.modifiers,
            tooltip: l("menu.close_tt")
        },

        // Edit section
        ...edit_base,

        // Edit entries
        "edit-undo": {
            label: l("menu.undo"),
            tooltip: l("menu.undo_tt"),
            key: pdbundle.shortcuts.menu.undo.key,
            modifiers: pdbundle.shortcuts.menu.undo.modifiers
        },
        "edit-redo": {
            label: l("menu.redo"),
            tooltip: l("menu.redo_tt"),
            key: pdbundle.shortcuts.menu.redo.key,
            modifiers: pdbundle.shortcuts.menu.redo.modifiers,
            bottom_hr: {}
        },
        "edit-cut": {
            label: l("menu.cut"),
            key: pdbundle.shortcuts.menu.cut.key,
            modifiers: pdbundle.shortcuts.menu.cut.modifiers,
            tooltip: l("menu.cut_tt")
        },
        "edit-paste": {
            label: l("menu.paste"),
            key: pdbundle.shortcuts.menu.paste.key,
            modifiers: pdbundle.shortcuts.menu.paste.modifiers,
            tooltip: l("menu.paste_tt")
        },
        "edit-paste-clipboard": {
            label: l("menu.paste_clipboard"),
            key: pdbundle.shortcuts.menu.paste_clipboard.key,
            modifiers: pdbundle.shortcuts.menu.paste_clipboard.modifiers,
            tooltip: l("menu.paste_clipboard_tt")
        },
        "edit-duplicate": {
            label: l("menu.duplicate"),
            key: pdbundle.shortcuts.menu.duplicate.key,
            modifiers: pdbundle.shortcuts.menu.duplicate.modifiers,
            tooltip: l("menu.duplicate_tt")
        },
        "edit-reselect": {
            label: l("menu.reselect"),
            key: pdbundle.shortcuts.menu.reselect.key,
            modifiers: pdbundle.shortcuts.menu.reselect.modifiers,
            tooltip: l("menu.reselect_tt")
        },
        "edit-tidyup": {
            label: l("menu.tidyup"),
            key: pdbundle.shortcuts.menu.tidyup.key,
            modifiers: pdbundle.shortcuts.menu.tidyup.modifiers,
            tooltip: l("menu.tidyup_tt")
        },
        "edit-font": {
            label: l("menu.font"),
            tooltip: l("menu.font_tt")
        },
        "edit-cordinspector": {
            label: l("menu.cordinspector"),
            key: pdbundle.shortcuts.menu.cordinspector.key,
            modifiers: pdbundle.shortcuts.menu.cordinspector.modifiers,
            tooltip: l("menu.cordinspector_tt"),
            bottom_hr: {}            
        },
        "edit-findagain": {
            label: l("menu.findagain"),
            key: pdbundle.shortcuts.menu.findagain.key,
            modifiers: pdbundle.shortcuts.menu.findagain.modifiers,
            tooltip: l("menu.findagain")
        },
        "edit-finderror": {
            action: {onclick: function(){pdbundle.pdgui.pdsend("pd finderror")}},
            label: l("menu.finderror"),
            tooltip: l("menu.finderror_tt"),
            bottom_hr: {}            
        },

        "edit-editmode": {
            label: l("menu.editmode"),
            key: pdbundle.shortcuts.menu.editmode.key,
            modifiers: pdbundle.shortcuts.menu.editmode.modifiers,
            tooltip: l("menu.editmode_tt"),
            bottom_hr: {}
        },

        // View section
        ...view_base,

        // View entries
        "view-optimalzoom": {
            label: l("menu.zoomoptimal"),
            key: pdbundle.shortcuts.menu.zoomoptimal.key,
            modifiers: pdbundle.shortcuts.menu.zoomoptimal.modifiers,
            tooltip: l("menu.zoomoptimal_tt")
        },
        "view-horizzoom": {
            label: l("menu.zoomhoriz"),
            key: pdbundle.shortcuts.menu.zoomhoriz.key,
            modifiers: pdbundle.shortcuts.menu.zoomhoriz.modifiers,
            tooltip: l("menu.zoomhoriz_tt")
        },
        "view-vertzoom": {
            label: l("menu.zoomvert"),
            key: pdbundle.shortcuts.menu.zoomvert.key,
            modifiers: pdbundle.shortcuts.menu.zoomvert.modifiers,
            tooltip: l("menu.zoomvert_tt")
        },

        // Put section
        "menu-put": {label: l("menu.put")},

        // Put entries
        "put-object": {
            label: l("menu.object"),
            key: pdbundle.shortcuts.menu.object.key,
            modifiers: pdbundle.shortcuts.menu.object.modifiers,
            tooltip: l("menu.object_tt")
        },
        "put-msgbox": {
            label: l("menu.msgbox"),
            key: pdbundle.shortcuts.menu.msgbox.key,
            modifiers: pdbundle.shortcuts.menu.msgbox.modifiers,
            tooltip: l("menu.msgbox_tt")
        },
        "put-number": {
            label: l("menu.number"),
            key: pdbundle.shortcuts.menu.number.key,
            modifiers: pdbundle.shortcuts.menu.number.modifiers,
            tooltip: l("menu.number_tt")
        },
        "put-symbol": {
            label: l("menu.symbol"),
            key: pdbundle.shortcuts.menu.symbol.key,
            modifiers: pdbundle.shortcuts.menu.symbol.modifiers,
            tooltip: l("menu.symbol_tt")
        },
        "put-comment": {
            label: l("menu.comment"),
            key: pdbundle.shortcuts.menu.comment.key,
            modifiers: pdbundle.shortcuts.menu.comment.modifiers,
            tooltip: l("menu.comment_tt")
        },
        "put-dropdown": {
            label: l("menu.dropdown"),
            key: pdbundle.shortcuts.menu.dropdown.key,
            modifiers: pdbundle.shortcuts.menu.dropdown.modifiers,
            tooltip: l("menu.dropdown_tt"),
            bottom_hr: {}            
        },
        "put-bang": {
            label: l("menu.bang"),
            key: pdbundle.shortcuts.menu.bang.key,
            modifiers: pdbundle.shortcuts.menu.bang.modifiers,
            tooltip: l("menu.bang_tt")
        },
        "put-toggle": {
            label: l("menu.toggle"),
            key: pdbundle.shortcuts.menu.toggle.key,
            modifiers: pdbundle.shortcuts.menu.toggle.modifiers,
            tooltip: l("menu.toggle_tt")
        },
        "put-number2": {
            label: l("menu.number2"),
            key: pdbundle.shortcuts.menu.number2.key,
            modifiers: pdbundle.shortcuts.menu.number2.modifiers,
            tooltip: l("menu.number2")
        },
        "put-vslider": {
            label: l("menu.vslider"),
            key: pdbundle.shortcuts.menu.vslider.key,
            modifiers: pdbundle.shortcuts.menu.vslider.modifiers,
            tooltip: l("menu.vslider_tt")
        },
        "put-hslider": {
            label: l("menu.hslider"),
            key: pdbundle.shortcuts.menu.hslider.key,
            modifiers: pdbundle.shortcuts.menu.hslider.modifiers,
            tooltip: l("menu.hslider_tt")
        },
        "put-vradio": {
            label: l("menu.vradio"),
            key: pdbundle.shortcuts.menu.vradio.key,
            modifiers: pdbundle.shortcuts.menu.vradio.modifiers,
            tooltip: l("menu.vradio_tt")
        },
        "put-hradio": {
            label: l("menu.hradio"),
            key: pdbundle.shortcuts.menu.hradio.key,
            modifiers: pdbundle.shortcuts.menu.hradio.modifiers,
            tooltip: l("menu.hradio_tt")
        },
        "put-vu": {
            label: l("menu.vu"),
            key: pdbundle.shortcuts.menu.vu.key,
            modifiers: pdbundle.shortcuts.menu.vu.modifiers,
            tooltip: l("menu.vu_tt")
        },
        "put-cnv": {
            label: l("menu.cnv"),
            key: pdbundle.shortcuts.menu.cnv.key,
            modifiers: pdbundle.shortcuts.menu.cnv.modifiers,
            tooltip: l("menu.cnv_tt"),
            bottom_hr: {}
        },
        "put-array": {
            label: l("menu.array"),
            tooltip: l("menu.array_tt")
        },

        // Window section 
        ...window_base,

        // Window entries
        "window-parentwin" : {
            label: l("menu.parentwin"),
            tooltip: l("menu.parentwin_tt"),
            top_hr: {}
        },
        "window-visible-ancestor" : {
            label: l("menu.visible_ancestor"),
            tooltip: l("menu.visible_ancestor_tt")
        },
        "window-pdwin" : {
            action: {onclick: function(){pdbundle.pdgui.raise_pd_window()}},
            label: l("menu.pdwin"),
            tooltip: l("menu.pdwin_tt"),
            key: pdbundle.shortcuts.menu.pdwin.key,
            modifiers: pdbundle.shortcuts.menu.pdwin.modifiers
        }
    };

    var canvas_menu = {
        ...console_menu,
        ...canvas_menu_base,
    }

    var web_menu = {
        ...console_menu_base,
    };

    var web_canvas = {
        ...canvas_menu_base,
        "file-download": {
            label: l("menu.download"),
        },
    }


    var entries = {
        "console": console_menu,
        "canvas" : canvas_menu,
        "web": web_menu,
        "web-canvas": web_canvas
    }

    // Iteracting over the menu options to set it's args and make it visible
    var menu = entries[type];
    if (cid === undefined) cid = "";
    tmp_cid = cid;

    if(cid != ""){
        window.shortkeys[cid] = {...window.shortkeys[""]}
    }else{
        window.shortkeys[cid] = {}
    }
    
    for (const id in menu) {
        var menu_item =  w.document.getElementById(id+cid);
        visible(menu_item);
        remove_select_text(menu_item);
        for (const option in menu[id]) {
            window[option](menu_item, menu[id][option])
        }
    }

    return menu;
}


// Functions set on args
function visible(menu_item){
    menu_item.style.display = "block";
}

function remove_select_text(menu_item){
    menu_item.onselectstart = function(e){
        e.preventDefault();
    }
}

window.action = function(menu_item, actions){
    for (var key in actions) {
        if (actions.hasOwnProperty(key)) {
            menu_item[key] = actions[key];
        }
    }
}

window.label = function(menu_item, label){
    menu_item.prepend(label);
}

// Help vars for shortkey implementation
window.shortkeys = {}
var tmp_key;
var tmp_cid;

window.key = function(menu_item, shortkey){ 
    tmp_key = shortkey;
}

window.modifiers = function(menu_item, modifiers){    
    var shortkey = modifiers + "+" + tmp_key;
    window.shortkeys[[tmp_cid]][[shortkey]] = menu_item;

    var span = document.createElement("span");
    span.append(shortkey)
    menu_item.append(span);
}

window.tooltip = function(menu_item, tooltip){
    // console.log("NEED TO IMPLEMENT TOOLTIP", tooltip);
}

window.recent_files = function(elem){
    var holder = document.createElement("ul");
    pdbundle.pdgui.populate_recent_files(holder)
    elem.append(holder)
}

function elem_hr(){
    var li = document.createElement("li");
    li.setAttribute("class", "hr"); 
    var hr = document.createElement("hr");
    li.append(hr)
    
    return li
}

window.top_hr = function(menu_item, hr){
    var hr = elem_hr();
    visible(hr);
    menu_item.before(hr)
}

window.bottom_hr = function(menu_item, hr){
    var hr = elem_hr();
    visible(hr);
    menu_item.after(hr)
}

// Common functions for both menus
function open_file(w){
    var input, chooser,
        span = w.document.querySelector("#fileDialogSpan");
    // Complicated workaround-- see comment in build_file_dialog_string
    input = pdbundle.pdgui.build_file_dialog_string({
        style: "display: none;",
        type: "file",
        id: "fileDialog",
        nwworkingdir: pdbundle.pdgui.get_pd_opendir(),
        multiple: null,
        // These are copied from pd_filetypes in pdbundle.pdgui.js
        accept: ".pd,.pat,.mxt,.mxb,.help"
    });
    span.innerHTML = input;
    chooser = w.document.querySelector("#fileDialog");
    // Hack-- we have to set the event listener here because we
    // changed out the innerHTML above
    chooser.onchange = function() {
        var file_array = this.value;
        // reset value so that we can open the same file twice
        this.value = null;
        pdbundle.pdgui.menu_open(file_array);
        console.log("tried to open something");
    };
    chooser.click();
}