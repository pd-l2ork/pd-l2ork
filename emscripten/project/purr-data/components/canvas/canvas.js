"use strict";

function redefine_id(){
    console.log("REDEFINE");
}

window.register_canvas = function(cid){
    console.log("CID", cid);

    // Update patch id
    var patch = document.getElementById("patch")
    patch.id = patch.id + cid;

    // Update patch_div id
    var patch_div = document.getElementById("patch_div_")
    patch_div.id = patch_div.id + cid;
    

    // Update patchsvg_ id
    var patchsvg = document.getElementById("patchsvg_")
    patchsvg.id = patchsvg.id + cid;

    // Update patchsvg_holder id
    var patchsvg_holder = document.getElementById("patchsvg_holder_")
    patchsvg_holder.id = patchsvg_holder.id + cid;
    
    // Update cordinspector id
    var cordinspector = document.getElementById("cordinspector")
    cordinspector.id = cordinspector.id + cid;

    // Update patchsvg_holder id
    var editmode = document.getElementById("editmode")
    editmode.id = editmode.id + cid;
    
    // Remain elements
    var elems = document.getElementById("patch"+cid).getElementsByTagName("li");
    for (const elem of elems) {
        elem.id = elem.id + cid
    }
}