# Custom Pages using WebPdL2Ork

This folder can be used as an example of how to create custom pages using WebPdL2Ork.

## index.html

This file contains the boilerplate code for creating WebPdL2Ork pages. Use the comments in this file to guide the creation of pages. There are certain HTML tags which are required to embed WebPdL2Ork into a page, and other which can be added/removed/modified as needed. All of these details are marked in the inline comments.

## index.js

This file contains the custom code for the page, which controls the action of the demo button and the response to data hooks. Use this as an example for connecting your own JavaScript to WebPdL2Ork.

## index.css

This file contains the styling for the page. Use this as an example for controlling the placement of the embedded WebPdL2Ork on the page.

## demo.pd

A custom patch. Custom pages can force the use of their own patches, or they can allow any patch to be loaded. The javascript forces the use of this "demo.pd" which can be viewed here.