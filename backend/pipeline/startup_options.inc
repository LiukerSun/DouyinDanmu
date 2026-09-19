struct StartupOptions {
    bool debug=false,apply=false;
    std::string command;
};
static StartupOptions read_startup_options(const std::vector<std::string>& args) {
    StartupOptions options;
    if(args.empty())return options;
    if(args.size()==1 && args[0]=="--debug"){options.debug=true;return options;}
    if((args[0]=="--replay-quarantine" || args[0]=="--redecode-details" || args[0]=="--repair-gift-facts") &&
       (args.size()==1 || (args.size()==2 && args[1]=="--apply"))) {
        options.command=args[0];options.apply=args.size()==2;return options;
    }
    throw std::invalid_argument("Usage: douyin-pipeline [--debug] | [--replay-quarantine|--redecode-details|--repair-gift-facts [--apply]]");
}

// Runtime opt-in applies to both the UI and exported detail responses. Stored
// diagnostics stay intact so they can be inspected after restarting with --debug.
static json message_detail_response(json stored,bool debug) {
    if(stored.is_null())return stored;
    stored["debug_mode"]=debug;
    if(!debug)stored["details"]=nullptr;
    return stored;
}
