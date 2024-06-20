// do not remove this line since you are not allowed to use unsafe code
#![deny(unsafe_code)]

// a similar 'die' macro with the C version
macro_rules! die {
    ($($arg:tt)*) => {
        eprintln!("proj3: {}", format_args!($($arg)*));
        panic!();
    };
}

use std::panic;

use std::fs::File;
use std::io::{self, Read, stdin};


//helpers!!

fn is_brace_balanced(s: &str, index: usize) -> bool {
    if index >= s.len() {
        return false; // Index out of bounds
    }

    let bytes = s.as_bytes();
    let mut left_brace_count = 0;
    let mut right_brace_count = 0;
    let mut i = index;

    while i < s.len() {
        match bytes[i] {
            b'{' => {
                // Count the number of consecutive backslashes before '{'
                let mut backslash_count = 0;
                while i > 0 && bytes[i - 1 - backslash_count] == b'\\' && backslash_count + 1 < i {
                    backslash_count += 1;
                }

                // If the number of consecutive backslashes is even, count the brace
                if backslash_count % 2 == 0 {
                    left_brace_count += 1;
                }
            },
            b'}' => {
                // Count the number of consecutive backslashes before '}'
                let mut backslash_count = 0;
                while i > 0 && bytes[i - 1 - backslash_count] == b'\\' && backslash_count + 1 < i {
                    backslash_count += 1;
                }

                // If the number of consecutive backslashes is even, count the brace
                if backslash_count % 2 == 0 {
                    right_brace_count += 1;
                    if right_brace_count > left_brace_count {
                        return false; // Not balanced
                    }
                }
            },
            _ => {}
        }

        if left_brace_count > 0 && left_brace_count == right_brace_count {
            // Found a balanced set of braces
            return true;
        }

        i += 1;
    }

    false // Not balanced
}


fn brace_balance_end(s: &str, index: usize) -> usize {
    let bytes = s.as_bytes();
    let mut left_brace_count = 0;
    let mut right_brace_count = 0;
    let mut i = index;

    while i < s.len() {
        let c = bytes[i];

        if c == b'{' && (i == 0 || (bytes[i - 1] != b'\\' && bytes[i - 1] != b'%')) {
            left_brace_count += 1;
        } else if c == b'{' && bytes[i - 1] == b'\\' && i != 1 && bytes[i - 2] == b'\\' {
            left_brace_count += 1;
        } else if c == b'}' && (i == 0 || (bytes[i - 1] != b'\\' && bytes[i - 1] != b'%')) {
            right_brace_count += 1;
            if right_brace_count > left_brace_count {
                return usize::MAX;
            }
            if left_brace_count == right_brace_count {
                return i;
            }
        } else if c == b'}' && bytes[i - 1] == b'\\' && i != 1 && bytes[i - 2] == b'\\' {
            right_brace_count += 1;
            if right_brace_count > left_brace_count {
                return usize::MAX;
            }
            if left_brace_count == right_brace_count {
                return i;
            }
        }

        i += 1;
    }

    return usize::MAX; // Indicating no balanced set was found
}

struct MacroNode {
    name: String,
    value: String,
    next: Option<Box<MacroNode>>,
}

impl MacroNode {
    fn new(name: &str, value: &str) -> Box<MacroNode> {
        Box::new(MacroNode {
            name: name.to_string(),
            value: value.to_string(),
            next: None,
        })
    }
}

fn macro_exists(head: &Option<Box<MacroNode>>, name: &str) -> bool {
    let mut current = head;
    while let Some(ref node) = current {
        if node.name == name {
            return true; // Found the macro with the given name.
        }
        current = &node.next; // Move to the next node.
    }
    false // Reached the end of the list without finding the macro.
}

fn add_or_update_macro(head: &mut Option<Box<MacroNode>>, name: &str, value: &str) {
    let mut current = head;
    while let Some(ref mut node) = current {
        if node.name == name {
            node.value = value.to_string();
            return;
        }
        current = &mut node.next;
    }

    let new_node = MacroNode::new(name, value);
    *current = Some(new_node);
}

fn remove_macro(head: &mut Option<Box<MacroNode>>, name: &str) {
    let mut current = head;
    while current.is_some() {
        if current.as_ref().unwrap().name == name {
            let next = current.take().unwrap().next; // Take the current node out, moving `next` in place.
            *current = next; // Reassign `current` directly with the next node.
            return;
        } else {
            current = &mut current.as_mut().unwrap().next;
        }
    }
}

fn find_macro_value(head: &Option<Box<MacroNode>>, name: &str) -> Option<String> {
    let mut current = head;
    while let Some(ref node) = current {
        if node.name == name {
            return Some(node.value.clone());
        }
        current = &node.next;
    }
    None
}

fn process_comments(input: &str) -> String {
    let mut output = String::new();
    let mut backslash_count = 0;
    let mut in_comment = false;

    let mut after_comment = false;

    let mut last_char_was_backslash = false;

    for c in input.chars() {


        if c == '\\' {
            //FIXED
            if !in_comment
            {
                backslash_count += 1;
                last_char_was_backslash = true;
            }
        } else {
            if c == '%' {
                if backslash_count % 2 == 1 {
                    // Odd number of backslashes before '%'
                    output.push('\\');

                    for _b in 0..((backslash_count - 1)/2)
                    {
                        output.push('\\');
                    }
                    
                    if !in_comment {
                        // if not in a comment, treat '%' as literal because it's escaped
                        output.push('%');
                    } else if last_char_was_backslash {
                        // in comment and '%' is escaped, do not add it to output
                        // remove the incorrectly added backslash in previous iteration
                        if output.ends_with('\\') {
                            output.pop();
                        }
                    }

                } else {
                    // Even number of backslashes, or no backslashes
                    if !in_comment {
                        in_comment = true; // Start comment mode only if not already in comment
                    } else if last_char_was_backslash {
                        // In comment and '%' is escaped, ensure it's not added to output
                    }

                    for _b in 0..(backslash_count)
                    {
                        output.push('\\');
                    }

                }
                backslash_count = 0; // reset backslash count after handling '%'
                last_char_was_backslash = false; // reset last character
            } else {

                if backslash_count > 0 {
                    // handle backslashes before a normal character
                    output.push_str(&"\\".repeat(backslash_count));
                    backslash_count = 0;
                }

                if after_comment {
                    if c == '\t' || c == ' '
                    {
                        continue;
                    }
                    else
                    {
                        after_comment = false;
                    }
                }

                if !in_comment || (c == '\n' && last_char_was_backslash) {
                    output.push(c); 
                }

                if c == '\n' {

                    if in_comment
                    {
                        after_comment = true;
                    }

                    in_comment = false; // end comment mode on newline
                }

                last_char_was_backslash = false; // reset last character
            }
        }
    }

    // handle remaining backslashes at the end of input if there are  any
    if backslash_count > 0 {
        output.push_str(&"\\".repeat(backslash_count));
    }

    output
}



#[derive(PartialEq)]
enum StateBks {
    InitialBks,
    EscapeBks,
}

//PROCESS ESCAPES
fn process_backslashes(input: &str, _head: &mut Option<Box<MacroNode>>) -> String {

    let mut current_state = StateBks::InitialBks;
    let mut output = String::new();
    let mut backslash_count = 0;

    let _stop = 0;

    //DO I NEED THIS
    //probably
    let bytes = input.as_bytes(); // convert the string to a byte slice


    let mut i = 0usize;

    while i < input.len() {
        let c = bytes[i] as char;

        match current_state
        {
            StateBks::InitialBks => {
                backslash_count = 0;

                if c == '\\'
                {
                    current_state = StateBks::EscapeBks;
                    backslash_count += 1;
                }
                else
                {
                    output.push(c);
                }

            }

            StateBks::EscapeBks => {
                if c == '\\' || c == '#' || c == '%' || c == '{' || c == '}'
                {
                    if c == '\\'
                    {
                        backslash_count += 1;


                        for _b in 0..(backslash_count / 2)
                        {
                            output.push('\\');
                        }

                        for _b in 0..(backslash_count % 2)
                        {
                            output.push('\\');
                        }
                    }
                    else
                    {
                        output.push(c);
                    }

                    current_state = StateBks::InitialBks;
                }
                else if !c.is_alphanumeric()
                {
                    //preserve \s
                    output.push('\\');
                    output.push(c);

                    current_state = StateBks::InitialBks;
                }
                //if even backslashes then macro is useless
                else if c.is_alphanumeric() && backslash_count % 2 == 0
                {
                    for _b in 0..(backslash_count / 2)
                    {
                        output.push('\\');
                    }

                    output.push(c);

                    current_state = StateBks::InitialBks;

                    //macro useless, just output as plaintext
                }
            }
        }

        i += 1;
    }
    return output;
}


fn replace_hash_with_arg(macro_value: &str, arg: &str) -> String {
    let mut result = String::new();
    let mut chars = macro_value.chars().peekable();
    let mut backslash_count = 0;

    while let Some(c) = chars.next() {
        match c {
            '#' => {
                if backslash_count % 2 == 0 {
                    // if there are an even number of backslashes (including 0), replace '#' with arg
                    // correctly handle even backslashes by preserving (!!!!) them
                    for _ in 0..backslash_count {
                        result.push('\\');
                    }
                    result.push_str(arg);
                } else {
                    // if there's an odd number of backslashes, it means '#' is escaped
                    // preserve all backslashes
                    for _ in 0..backslash_count {
                        result.push('\\');
                    }
                    // add '#' bc the last backslash escapes it
                    result.push('#');
                }
                backslash_count = 0; // reset backslash count
            },
            '\\' => {
                backslash_count += 1;
            },
            _ => {
                // every backslash should be preserved!! will go through other machine later
                for _ in 0..backslash_count {
                    result.push('\\');
                }
                backslash_count = 0; // reset backslash count
                result.push(c); 
            }
        }
    }

    // handle any remaining backslashes at the end of string
    for _ in 0..backslash_count {
        result.push('\\');
    }

    result
}



#[derive(PartialEq)]
enum State {
    Initial,
    Escape,
    GenMacro,
    Def,
    UserDef,
    UnDef,
    If,
    IfDef,
    Include,
    ExpandAfter
}

fn process(input: &str, head: &mut Option<Box<MacroNode>>, initial_string: &str) -> String {

    let mut current_state = State::Initial;
    let mut output = String::new();
    let mut backslash_count = 0;


    let _temp = String::new();

    let mut arg1 = String::new();
    let mut arg2 = String::new();
    let mut arg3 = String::new();

    let mut argspecial = String::new();

    let mut stop;

    let bytes = input.as_bytes(); 

    let mut i = 0usize;

    while i < input.len() {
        let c = bytes[i] as char;

        match current_state
        {
            State::Initial => {
                backslash_count = 0;

                if c == '\\'
                {
                    current_state = State::Escape;
                    backslash_count += 1;
                }
                else
                {
                    output.push(c);
                }

            }

            State::Escape => {
                if c == '\\'
                {
                    if backslash_count == 1
                    {
                        output.push('\\');
                    }

                    backslash_count += 1;

                    output.push('\\');

                    current_state = State::Escape;
                }
                else if c == '{' || c == '}'
                {
                    if backslash_count == 1
                    {
                        output.push('\\');
                    }

                    output.push(c);

                    current_state = State::Initial;
                }
                //if even backslashes then macro is useless
                else if c.is_alphanumeric() && backslash_count % 2 == 1
                {

                    i -= 1;

                    current_state = State::GenMacro;

                    //macro useless, just output as plaintext
                }
                else if c.is_alphanumeric() && backslash_count % 2 == 0
                {
                    if backslash_count == 1
                    {
                        output.push('\\');
                    }

                    current_state = State::Initial;

                    output.push(c);
                    //macro useless, just output as plaintext
                }
                else
                {
                    if c == '#'
                    {
                        if backslash_count == 1
                        {
                            output.push('\\');
                        }

                        output.push('#');

                        current_state = State::Initial;
                    }
                    else
                    {
                        if backslash_count == 1
                        {
                            output.push('\\');
                        }

                        output.push(c);
                        current_state = State::Initial;
                    }
                }
                //break;
            }

            State::GenMacro => {

                if i + 5 < input.len() && &input[i..=i+5] == "ifdef{" {
                    i += 4;
                    current_state = State::IfDef;
                } 
                else if i + 3 < input.len() && &input[i..=i+3] == "def{" {
                    i += 2;
                    current_state = State::Def;
                } else if i + 5 < input.len() && &input[i..=i+5] == "undef{" {
                    i += 4;
                    current_state = State::UnDef;
                } 
                else if i + 2 < input.len() && &input[i..=i+2] == "if{" {
                    i += 1;
                    current_state = State::If;
                } 
                else if i + 7 < input.len() && &input[i..=i+7] == "include{" {
                    i += 6;
                    current_state = State::Include;
                } 
                else if i + 11 < input.len() && &input[i..=i+11] == "expandafter{" {
                    i += 10;
                    current_state = State::ExpandAfter;
                }
                else
                {
                    //user def
                    let start = i;

                    while i < input.len() && input[i..=i].chars().next().map_or(false, |c| c.is_alphanumeric()) {
                        i += 1;
                    }

                    argspecial.clear();

                    // extend argspecial with the slice from input
                    argspecial.extend(input[start..i].chars());

                    if !macro_exists(head, &argspecial) {
                        die!("user-def arg not defined: {}", argspecial);
                    }

                    i -= 1;

                    current_state = State::UserDef;
                }

            }

            State::Def => {

                if c != '{'
                {
                    //ERRCHANGE
                    die!("missing name");
                }

                if !is_brace_balanced(input, i) {
                    die!("not brace balanced");
                }

                // ensure the first argument is correctly started
                let stop = brace_balance_end(input, i);

                arg1.clear();
                // process the first argument
                arg1.extend(input[i+1..stop].chars());

                //check name
                if arg1.is_empty() {
                    // return an error if name is empty
                    die!("The name cant be empty");
                }
            
                if !arg1.chars().all(|c| c.is_alphanumeric()) {
                    // return an error
                    die!("The name must be alphanumeric");
                }

                // move past the first argument's closing brace
                i = stop + 1;

                // Ensure there's a starting brace for the second argument
                if i >= input.len() || input.as_bytes()[i] != b'{' {
                    die!("missing value");
                }

                if !is_brace_balanced(input, i) {
                    die!("not brace balanced or too many arguments");
                }

                // process the second argument

                let stop = brace_balance_end(input, i);

                arg2.clear();
                arg2.extend(input[i+1..stop].chars());

                // move past the second argument's closing brace
                i = stop + 1;

                // check if macro already exists
                if find_macro_value(head, &arg1).is_some() {
                    die!("Macro '{}' already defined", arg1);
                } 

                i -= 1;

                // add or update the macro
                add_or_update_macro(head, &arg1, &arg2);

                current_state = State::Initial;

                arg1.clear();
                arg2.clear();

            }

            State::UnDef => {

                if c != '{'
                {
                    //ERRCHANGE
                    die!("missing name");
                }

                if !is_brace_balanced(input, i) {
                    die!("not brace balanced");
                }

                // ensure the first argument is correctly started
                let stop = brace_balance_end(input, i);

                //NEED TO CLEAR BEFORE EXTEND
                arg1.clear();
                // process the first argument
                arg1.extend(input[i+1..stop].chars());

                // move past the first argument's closing brace
                i = stop;


                // check if macro already exists
                if find_macro_value(head, &arg1).is_some() {
                    remove_macro(head, &arg1);
                }
                else
                {
                    //cant undef undefined macro
                    die!("can't undef an undefined macro");
                }
                

                current_state = State::Initial;

                arg1.clear();
            }

            State::UserDef => {

                // retrieve the macro value 
                let macro_value = match find_macro_value(head, &argspecial) {
                    Some(value) => value,
                    None => {
                        die!("Macro '{}' not found", argspecial);
                    }
                };

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("Syntax error or missing argument");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                let end_brace = brace_balance_end(input, i);

                //just in case
                arg3.clear();
                arg3.extend(input[i+1..end_brace].chars());

                let mut expanded_macro = replace_hash_with_arg(&macro_value, &arg3);

                //end brace shouldn't have changed

                expanded_macro.extend(input[end_brace+1..input.len()].chars());

                //processing rest of it

                let mut processed_arg = String::new();
                
                let mut final_arg = process(&expanded_macro, head, initial_string);

                output.push_str(&final_arg);

                arg3.clear();
                argspecial.clear();

                expanded_macro.clear();
                processed_arg.clear();

                final_arg.clear();
                
                return output;
            }

            State::If => {

                if c != '{' {
                    die!("missing condition");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg1.clear();
                arg1.extend(input[i+1..stop].chars());

                //SECOND ARG

                //this should NOW be pointing to right after the first brace pair
                i = stop + 1;

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("missing then");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);


                arg2.clear();
                arg2.extend(input[i+1..stop].chars());

                //now pointing to right after the second brace pair
                i = stop + 1;

                //THIRD ARG

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("missing else");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg3.clear();
                arg3.extend(input[i+1..stop].chars());

                let index_holder_if = stop;


                // determine content based on arg1's condition (non-empty means true)
                let mut content = if !arg1.is_empty() {
                    arg2.clone() // clone arg2 if condition is true
                } else {
                    arg3.clone() //false
                };


                content.extend(input[index_holder_if+1..].chars());

                content = process(&content, head, initial_string);


                // add the result to the output
                output.push_str(&content);

                content.clear();

                return output;

            }


            State::IfDef => {

                if c != '{' {
                    die!("missing condition");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg1.clear();
                arg1.extend(input[i+1..stop].chars());

                //SECOND ARG

                //this should NOW be pointing to right after the first brace pair
                i = stop + 1;

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("missing then");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg2.clear();
                arg2.extend(input[i+1..stop].chars());

                //now pointing to right after the second brace pair
                i = stop + 1;

                //THIRD ARG

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("missing else");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg3.clear();
                arg3.extend(input[i+1..stop].chars());


                let index_holder23 = stop; 


                // Determine the content based on arg1's condition (non-empty means true)
                let condition_met = find_macro_value(head, &arg1).is_some();


                let mut content_ifdef = if condition_met {
                    arg2.clone() // Clone to get a mutable copy
                } else {
                    arg3.clone() // Clone to get a mutable copy
                };

                content_ifdef.extend(input[index_holder23+1..].chars());

                content_ifdef = process(&content_ifdef, head, initial_string);

                // add the result to the output
                output.push_str(&content_ifdef);

                content_ifdef.clear();

                return output;

            }

            State::ExpandAfter => {

                if c != '{' {
                    die!("missing before");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg1.clear();
                arg1.extend(input[i+1..stop].chars());

                //SECOND ARG

                //this should NOW be pointing to right after the first brace pair
                i = stop + 1;

                if input.as_bytes().get(i) != Some(&b'{') {
                    die!("missing then");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg2.clear();
                arg2.extend(input[i+1..stop].chars());

                //now pointing to right after the second brace pair
                i = stop + 1;

                let hold_here = i;

                // process the second arg
                let result_after = process(&arg2, head, initial_string);

                // add the result to the arg1
                arg1.push_str(&result_after);

                arg1.extend(input[hold_here..].chars());

                // process the second arg and rest of string
                let result_before = process(&arg1, head, initial_string);

                // add the result to the output
                output.push_str(&result_before);

                //OUTPUT

                return output;

            }

            State::Include => {

                if c != '{' {
                    die!("missing before");
                }

                if !is_brace_balanced(input, i) {
                    die!("Not brace balanced");
                }

                stop = brace_balance_end(input, i);

                arg1.clear();

                for k in (i + 1)..stop {
                    let c = input.as_bytes()[k] as char; // Convert byte to char for comparison
                
                    // Check if the character is not allowed
                    if !(c.is_alphanumeric() || c == '_' || c == '/' || c == '-' || c == '.') {
                        die!("File name contains invalid characters.");
                    }
                
                    // add the character to arg1
                    arg1.push(c);
                }

                let path = arg1.trim(); // trim any  whitespace
                //RUST FUNCTION

                arg2 = initial_file_input(&path).expect("REASON");

                stop = brace_balance_end(input, i);

                //now pointing to right after the first brace pair
                i = stop + 1;

                let include_hold_here = i;


                // process the include
                //let result_after = process(&arg2, head);

                arg2.extend(input[include_hold_here..].chars());

                // Process the second arg and rest of string
                let result_winclude = process(&arg2, head, initial_string);

                // add the result to the output
                output.push_str(&result_winclude);

                //OUTPUT

                return output;

            }

        }
        //INCREMENT!
        i += 1;

    }

    return output;

}



//read in file input into a string and remove comments thru machine
fn initial_file_input(file_path: &str) -> io::Result<String> {

    match File::open(file_path) {
        Ok(mut file) => {
            let mut contents = String::new();
            file.read_to_string(&mut contents)?;
            let processed_contents = process_comments(&contents);
            Ok(processed_contents)
        },
        Err(_) => {
            die!("file dne on cml");
        }
    }
}


fn main() -> io::Result<()> {

    panic::set_hook(Box::new(|_| {}));


    let mut combined_contents = String::new();
    let args: Vec<String> = std::env::args().collect();

    if args.len() > 1 {
        // read from the files listed in the cml args
        for file_path in &args[1..] {
            let processed_contents = initial_file_input(file_path)?;
            combined_contents.push_str(&processed_contents);
        }
    } else {
        // no files specified; read from stdin
        let mut contents = String::new();
        stdin().read_to_string(&mut contents)?;
        // process the stdin contents and  remove comments
        //not using initial FILE input bc no file
        combined_contents = process_comments(&contents);
    }

    let mut my_head: Option<Box<MacroNode>> = None;

    let processed = process(&combined_contents, &mut my_head, &combined_contents);

    let processed_final = process_backslashes(&processed, &mut my_head);

    // process the concatenated contents
    print!("{}", processed_final);

    Ok(())
}
