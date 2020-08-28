"use strict";

window.update_canvas_id = function(cid){
    // Update patch id
    var patch = document.getElementById("patch")
    patch.id = patch.id + cid;
    
    // Remain elements
    var elems = document.getElementById("patch"+cid).getElementsByTagName("*");
    for (const elem of elems) {
        elem.id = elem.id + cid
    }
}

window.add_canvas_name = function(cid, name){
    // Update patch id
    var patch_filename = document.getElementById("patch-filename"+cid)
    
    // Remain elements
    patch_filename.innerHTML="";
    patch_filename.append(name);
}