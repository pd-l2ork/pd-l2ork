"use strict";

//to store indexes of characters
    function INDEX(x, y){
        this.x = x;
        this.y = y;
    };

//for tracking connection path
function tracking_connection(curr_row, curr_col, next_row, next_col ,
                            string_array ,visited_array) {
    var next_character,curr_character,end_index;
    curr_character = string_array[curr_row][curr_col];
    next_character = string_array[next_row][next_col];

    if (visited_array[next_row][next_col] == 0) {

        visited_array[next_row][next_col] = 1;

        switch(next_character) {
            case "|" : end_index = tracking_connection( next_row, next_col,
                                    next_row+1, next_col , string_array ,visited_array ); // go down
            break;
            case "\\" : end_index = tracking_connection( next_row, next_col,
                                    next_row+1, next_col+1 , string_array ,visited_array); // go bottom right
            break;
            case "/" : end_index = tracking_connection( next_row, next_col,
                                    next_row+1, next_col-1 , string_array ,visited_array); // go bottom left
            break;
            case "-" : if (next_col > curr_col) { // go in right direction
                end_index = tracking_connection( next_row, next_col,
                            next_row, next_col+1 , string_array ,visited_array);
            } else { // go in left direction
                end_index = tracking_connection( next_row, next_col,
                            next_row, next_col-1 , string_array ,visited_array);
            }
            break;
            case "+" : if (curr_character == "|") {
                if (string_array[next_row][next_col+1] == "-") {
                    // go in right direction
                    end_index = tracking_connection( next_row, next_col,
                                next_row, next_col+1 , string_array ,visited_array);
                } else if (string_array[next_row][next_col-1] == "-") {
                    // go in left direction
                    end_index = tracking_connection( next_row, next_col,
                                next_row, next_col-1 , string_array ,visited_array);
                }
            } else if (curr_character == "-") {
          // Character following "+" will be "|" and it point to down direction
                end_index = tracking_connection( next_row, next_col,
                            next_row+1, next_col , string_array ,visited_array);
            }
            break;

            case "X" : if (curr_character == "\\") {
                // go bottom right
                end_index = tracking_connection(next_row, next_col,
                            next_row+1, next_col+1 , string_array ,visited_array);
                // for the next use of "X"
                visited_array[next_row][next_col] = 0;
            } else if (curr_character == "/") {
                end_index = tracking_connection(next_row,next_col,
                            next_row+1, next_col-1); // go bottom left
                visited_array[next_row][next_column] = 0;
            }
            break;

            default : end_index = new INDEX(next_row, next_col);
                      //Not a connection object;
            break;
        }
    } else {
        end_index = new INDEX(next_row, next_col);
    }

    return end_index;

};

function parse_ascii_art(ascii_art) {

    var row=0,
      string_array = [],
      visited_array = [],
      connections_array = [],
      objects_connections_map = [],
      characters_stack = [],
      obj_msg_array = [],
      state = "valid",
      i,
      j,
      temp_index,
      start_index;

    string_array.push([]);
    visited_array.push([]);


    //Finding Objects and Msgbox
    for (i = 0, j = 0 ; i < ascii_art.length ; i++, j++) {


        
        //reading the character of string
        var temp = ascii_art[i],
            array = [],
            s_temp,
            start;

        string_array[row][j]=temp;
        visited_array[row][j]=0;
        //For new row
        if (temp == "\n") {
            row = row+1;
            j = -1;
            string_array.push([]);
            visited_array.push([]);
            array.push(i);
        } else {

            temp_index = new INDEX(row, j);

            //Start of the object/message
            if ((characters_stack.length == 0) && temp == "[") {
                characters_stack.push(1);
                //to store the start_index of object/msgbox
                start_index = temp_index;
                // to slice the string from start to end of object/msgbox
                start = i;
                state = "invalid";

            } else if (temp == "[") {
                //Brackets inside the objects/message
                characters_stack.push(1);

            } else if ((characters_stack.length == 1) && temp == "]") {
                //to identify object
                characters_stack.pop();

                if (characters_stack.length == 0) {
                    s_temp = ascii_art.slice(start+1,i);
                    obj_msg_array.push({index1:start_index, index2:temp_index,
                                         type:1, string:s_temp});
                    state ="valid";
                }
            } else if ((characters_stack.length == 1) && temp == "(") {
                //to identify message
                characters_stack.pop();

                if (characters_stack.length == 0) {
                    s_temp = ascii_art.slice(start+1,i);
                    obj_msg_array.push({index1:start_index, index2:temp_index,
                                         type:0, string:s_temp});
                    state = "valid";
                }
            } else if (temp == "]") {

                 characters_stack.pop();
                 state = "invalid";

            } else {
                state = "partial";
                continue;
            }
        }
    }

    // Finding the connection's end points
    for (i = 0 ; i < row ; i++) {
        for (j = 0 ; j < string_array[i].length ; j++) {
            // visited_array to travel a connection path only once
            var temp = string_array[i][j],
                flag = 1,
                end_index;

            if (visited_array[i][j] == 0) {
                switch(temp) {
                    case "|": start_index = new INDEX(i-1, j);
                        end_index = tracking_connection(i, j, i+1, j ,
                                                        string_array ,visited_array  );
                    break;
                    case "\\": start_index = new INDEX(i-1, j-1);
                        end_index = tracking_connection(i, j, i+1, j+1 , 
                                                        string_array ,visited_array );
                    break;
                    case "/": start_index = new INDEX(i-1, j+1);
                        end_index = tracking_connection(i ,j, i+1, j-1 , 
                                                        string_array ,visited_array);
                    break;
                    case "-": start_index = new INDEX(i, j-1);
                        end_index = tracking_connection(i ,j, i, j+1 , 
                                                        string_array ,visited_array);
                    break;
                    //Special Cases
                    //case '+': Any Connection will not start with Plus

                    // as a connection starting with X will
                    // be a single element connection
                    case "X": start_index = new INDEX(i, j);
                        end_index = new INDEX(i, j);
                        visited_array[i][j] = 1;
                    break;
                    default : flag = 0;
                            visited_array[i][j] = 1;
                    break;
                }
                visited_array[i][j] = 1;
            }

            if (flag == 1) {
                connections_array.push({index1:start_index, index2:end_index});
            }
        }
    }


    //Finding the mapping between connections and Objects/Messages
        for (i = 0 ; i< connections_array.length ; i++) {
            var start_index = connections_array[i].index1,
                end_index = connections_array[i].index2,
                max_distance1 = Infinity,
                max_distance2 = Infinity,
                inlet_object_index=0,
                outlet_object_index=0,
                start_point = 0,
                end_point = 0,
                state_flag = 0;

            for (j = 0 ; j < obj_msg_array.length ; j++) {
                var start = obj_msg_array[j].index1,
                    end = obj_msg_array[j].index2;

                if ( start_index.x == end.x ) {
                    if ( Math.min( Math.abs(start_index.y - start.y) ,
                      Math.abs(start_index.y - end.y) ) < max_distance1 ) {
                        outlet_object_index=j;
                        max_distance1 = Math.abs(start_index.y - start.y);
                        state_flag= state_flag+ 1;
                        if ( Math.abs(start_index.y - start.y) <  Math.abs(start_index.y - end.y) ) {
                            start_point = 0;
                        } else {
                            start_point = 1;
                        }
                  
                    }
                }

                if ( end_index.x == start.x ){
                    if ( Math.min( Math.abs(end_index.y - start.y) ,
                      Math.abs(end_index.y - end.y) ) < max_distance2 ) {
                        inlet_object_index=j;
                        max_distance2 = Math.abs(end_index.y - end.y);
                        state_flag= state_flag+ 1;
                        if ( Math.abs(start_index.y - start.y) <  Math.abs(start_index.y - end.y) ) {
                            end_point = 0;
                        } else {
                            end_point = 1;
                        }
                    }
                }

            }
            
            if (state_flag== 2)
            {
                state= "valid";
            } else {
                state = "invalid";
            }

            objects_connections_map.push({start_object:outlet_object_index,
                                          end_object:inlet_object_index,
                                          start_point:start_point,
                                          end_point:end_point});

        }

    var pd_message = "";

    for ( i = 0; i < obj_msg_array.length ; i++) {
        var type = obj_msg_array[i].type,
            x1 = obj_msg_array[i].index1.x,
            y1 = obj_msg_array[i].index1.y,
            str = obj_msg_array[i].string;

        str.replace(";\n"," ");

        var point_x = y1*20 + 100,
            point_y = x1*20 + 50;
        point_x = point_x.toString(),
        point_y = point_y.toString();

        if ( type == 1 ) {
            pd_message = pd_message + "#X obj " + point_x + " " + point_y + " " + str + ";\n";

        } else {
            pd_message = pd_message + "#X msg " + point_x + " " + point_y + " " + str + ";\n";
        }
    }


    for (i = 0; i < objects_connections_map.length ; i++) {
        str= "#X connect " + objects_connections_map[i].start_object + " "
                            + objects_connections_map[i].start_point + " "
                            + objects_connections_map[i].end_object + " " 
                            + objects_connections_map[i].end_point +  ";\n";
        pd_message = pd_message + str;
    }
    var parser_data = {pd_message:pd_message,
                      state:state};
    return parser_data;
}

exports.parse_ascii_art = parse_ascii_art;
