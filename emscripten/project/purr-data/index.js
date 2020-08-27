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

        if (new_win === pdbundle.pdgui.get_patchwin(cid)) {
            // Add canvas html file
			$.get("./components/canvas/"+f, function(data){
                 $("#canvas-container").append(data)
                 update_canvas_id(cid);
                 register_canvas(cid, attr_array);
            });
            
            // flag the window as loaded. We may want to wait until the
            // DOM window has finished loading for this.
            pdbundle.pdgui.set_window_finished_loading(cid);
        }else if(new_win === pdbundle.pdgui.get_dialogwin(cid)){
            // Add menu html file
			$.get("./components/dialogs/"+f, function(data){
                var dialog_div = new_win.window.document.getElementById("dialog-div");

                if(dialog_div === null){
                    dialog_div = new_win.window.document.createElement('div')
                    dialog_div.id = "dialog-div";
                }

                // cleaning dialog div
                dialog_div.innerHTML = "";

                $("#sidebar-body").prepend(dialog_div.outerHTML)
                $("#dialog-div").prepend(data)

                // initialize the dialog window                
                register_dialog(cid,attr_array);
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

function add_menu(){
    // Add menu html file
    $.get("./components/menu/menu.html", function (data) {
        $("#menu").prepend(data)
        create_pd_window_menus(null, window)
    });
}

function create_pd_window_menus(gui, w) {
    var type = "web";
    var m = menu_options(type, w);
    add_shortcuts();
    load_menu_actions();
}

// Init Function
function gui_init(win){
    // Init vars
    pdbundle.pdgui.set_pd_window(win);
    pdbundle.pdgui.set_new_window_fn(create_window);
    add_menu()
}

function initialize_webapp() {
    gui_init(window);
};
