// Define perfect parser
function perfect_parser (){}


// Defining create window
function create_window(cid, type, width, height, xpos, ypos, attr_array) {
    // todo: make a separate way to format the title for OSX
    var my_title;
    if (type === "pd_canvas") {
        my_title = pdbundle.pdgui.format_window_title(
            attr_array.name,
            attr_array.dirty,
            attr_array.args,
            attr_array.dir);
    } else {
        my_title = type;
    }
    var my_file =
        type === "pd_canvas" ? "pd_canvas.html" : "dialog_" + type + ".html";


    var eval_string = "register_window_id(" +
                    JSON.stringify(cid) + ", " +
                    JSON.stringify(attr_array) + ");";

    var pdWindow =  function(my_file, params, callback){
        var new_window = {params}
        new_window.window = window
        new_window.window.on = function (event, callback, record) {
            if (event == "loaded"){
                this.addEventListener('load', callback)
            }    
        }
        callback(new_window)

    }

    function init_pd_window(f, new_win) {
            
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
        if (new_win === pdbundle.pdgui.get_patchwin(cid)) {
            
            // Create patch holder
            var patch = new_win.window.document.createElement('div'); 
            patch.classList.add("patch");
            patch.id = "patch"+cid;

            // Create canvas menu
            var patch_menu_html = ''+
                    '<ul class="patch-menu" id="patch-menu">'+
                        // File section
                        '<li id="menu-file'+cid+'">'+
                            '<ul>'+
                                '<li id="file-save'+cid+'"></li>'+
                                '<li id="file-saveas'+cid+'"></li>'+
                                '<li id="file-download'+cid+'"></li>'+
                                '<li id="file-print'+cid+'"></li>'+                                
                                '<li id="file-message'+cid+'"></li>'+
                                '<li id="file-close'+cid+'"></li>'+
                            '</ul>'+
                        '</li>'+
                        // Edit section
                        '<li id="menu-edit'+cid+'">'+
                            '<ul>'+
                                '<li id="edit-undo'+cid+'"></li>'+
                                '<li id="edit-redo'+cid+'"></li>'+
                                '<li id="edit-cut'+cid+'"></li>'+
                                '<li id="edit-copy'+cid+'"></li>'+
                                '<li id="edit-paste'+cid+'"></li>'+
                                '<li id="edit-paste-clipboard'+cid+'"></li>'+
                                '<li id="edit-duplicate'+cid+'"></li>'+
                                '<li id="edit-selectall'+cid+'"></li>'+
                                '<li id="edit-reselect'+cid+'"></li>'+
                                '<li id="edit-tidyup'+cid+'"></li>'+
                                '<li id="edit-font'+cid+'"></li>'+
                                '<li id="edit-cordinspector'+cid+'"><input type="checkbox" id="cordinspector'+cid+'"></li>'+                                
                                '<li id="edit-find'+cid+'"></li>'+
                                '<li id="edit-findagain'+cid+'"></li>'+
                                '<li id="edit-finderror'+cid+'"></li>'+
                                '<li id="edit-editmode'+cid+'"><input type="checkbox" id="editmode'+cid+'"></li>'+
                            '</ul>'+
                        '</li>'+

                        //  View Section
                        '<li id="menu-view'+cid+'">'+
                            '<ul>'+
                                '<li id="view-zoomin'+cid+'"></li>'+
                                '<li id="view-zoomout'+cid+'"></li>'+
                                '<li id="view-zoomreset'+cid+'"></li>'+
                                '<li id="view-optimalzoom'+cid+'"></li>'+
                                '<li id="view-horizzoom'+cid+'"></li>'+
                                '<li id="view-vertzoom'+cid+'"></li>'+            
                                '<li id="view-fullscreen'+cid+'"></li>'+
                            '</ul>'+
                        '</li>'+
            
                        // Put Section
                        '<li id="menu-put'+cid+'">'+
                            '<ul>'+
                                '<li id="put-object'+cid+'"/li>'+
                                '<li id="put-msgbox'+cid+'"/li>'+
                                '<li id="put-number'+cid+'"/li>'+
                                '<li id="put-symbol'+cid+'"/li>'+
                                '<li id="put-comment'+cid+'"/li>'+
                                '<li id="put-dropdown'+cid+'"/li>'+
                                '<li id="put-bang'+cid+'"/li>'+
                                '<li id="put-toggle'+cid+'"/li>'+
                                '<li id="put-number2'+cid+'"/li>'+
                                '<li id="put-vslider'+cid+'"/li>'+
                                '<li id="put-hslider'+cid+'"/li>'+
                                '<li id="put-vradio'+cid+'"/li>'+
                                '<li id="put-hradio'+cid+'"/li>'+
                                '<li id="put-vu'+cid+'"/li>'+
                                '<li id="put-cnv'+cid+'"/li>'+
                                '<li id="put-array'+cid+'"/li>'+
                            '</ul>'+
                        '</li>'+

                        // Window section
                        '<li id="menu-window'+cid+'">'+
                            '<ul>'+
                                '<li id="window-nextwin'+cid+'"></li>'+
                                '<li id="window-prevwin'+cid+'"></li>'+
                                '<li id="window-parentwin'+cid+'"></li>'+
                                '<li id="window-visible-ancestor'+cid+'"></li>'+
                                '<li id="window-pdwin'+cid+'"></li>'+
                            '</ul>'+
                        '</li>'+
                    '</ul>';
            
               
            var patch_menu = new_win.window.document.createElement('div')
            patch_menu.innerHTML = patch_menu_html;
            patch.appendChild(patch_menu);  

            // Create canvas draw area
            var patchsvg_html = '<div class="patch_window_svg" id="patch_div_'+ cid + '"> ' +
                                '<svg xmlns="http://www.w3.org/2000/svg"' +
                                    'version="1.1"'+
                                    'id="patchsvg_'+ cid +'"' +
                                    'class="noselect"'+
                                    'viewBox="0 0 0 0">'+
                                '</svg>'+
                                '</div>';

            var patchsvg = new_win.window.document.createElement('div');
            patchsvg.id = "patchsvg_holder_"+cid;
            patchsvg.innerHTML = patchsvg_html;
            patch.appendChild(patchsvg);

            
            // Canvas find
            var canvas_find_html = '<div id="canvas_find'+cid+'"style="display:none;">'+
                                '<label><input type="text"'+
                                    'id="canvas_find_text'+cid+'"'+
                                    'name="canvas_find_text"'+
                                    'defaultValue="Search in Canvas"'+
                                    'style="width:10em"/>'+
                            '</label>'+
                            '<button type="button"'+
                                'id="canvas_find_button'+cid+'"'+
                                'data-i18n="[title]canvas.find.search_tt">'+
                                '<span data-i18n="canvas.find.search"></span>'+
                            '</button>'+
                            '</div>'

            var canvas_find = new_win.window.document.createElement('div')
            canvas_find.innerHTML = canvas_find_html;
            patch.appendChild(canvas_find);             
        
            // Add to canvas holder
            var canvas_container = new_win.window.document.getElementById("canvas-container");
            canvas_container.appendChild(patch);

            // initialize the window
            new_win.window.eval(eval_string);

            // flag the window as loaded. We may want to wait until the
            // DOM window has finished loading for this.
            pdbundle.pdgui.set_window_finished_loading(cid);
        }else if(new_win === pdbundle.pdgui.get_dialogwin(cid)){
            // Add menu html file
			$.get("./"+f, function(data){
                var dialog_div = new_win.window.document.createElement('div')
                var n_cid = cid;

                if(cid.indexOf('.') === 0){
                    n_cid = cid.substring(1,cid.length-1);
                    dialog_div.id = "dialog-div-"+n_cid;
                }

                dialog_div.id = "dialog-div-"+n_cid;
                $("#sidebar-body").prepend(dialog_div.outerHTML)
                $("#dialog-div-"+n_cid).prepend(data)

                // initialize the dialog window                
                window.register_dialog_id(cid,attr_array);
            });
        }else {
            // If the window is no longer loading, we need to go ahead
            // and remove the reference to it in the patchwin array.
            // Otherwise we get dangling references to closed windowsE vamo 
            // and other bugs...
            if (!pdbundle.pdgui.window_is_loading(cid)) {
                if (type === "pd_canvas") {
                    pdbundle.pdgui.set_patchwin(cid, undefined);
                } else {
                    pdbundle.pdgui.set_dialogwin(cid, undefined);
                }
            }
            new_win.window.close(true);
        }
    }

    pdWindow(my_file, {
        title: my_title,
        position: "center",
        focus: true,
        width: width,
        // We add 23 as a kludge to account for the menubar at the top of
        // the window.  Ideally we would just get rid of the canvas menu
        // altogether to simplify things. But we'd have to add some kind of
        // widget for the "Put" menu.
        height: height + 23,
        x: xpos,
        y: ypos
    }, function (new_win) {        
        
        if (type === "pd_canvas") {
            pdbundle.pdgui.set_patchwin(cid, new_win);
            init_pd_window(my_file, new_win);
        } else {
            pdbundle.pdgui.set_dialogwin(cid, new_win);
            init_pd_window(my_file, new_win);
        }
    });
}

function create_pd_window_menus(gui, w) {
    var type = "web";
    var m = menu_options(type, w);

}

// Init Function
function gui_init(win){
    // Redefine perfect_parser
    perfect_parser = pdbundle.pdgui.perfect_parser;

    // Init vars
    pdbundle.pdgui.set_pd_window(win);
    pdbundle.pdgui.init_module(Module);
    pdbundle.pdgui.set_new_window_fn(create_window);
    create_pd_window_menus(null, window)
    add_shortcuts();
}

window.onload = function() {
    gui_init(window);
    load_menu_actions();
};


