#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <ctime>

#include "pnapi.h"
#ifdef PNAPI_ASSERT_H /* inclusion check */
#error DO NOT include pnapi-assert.h
#endif

#include "cmdline.h"
#include "Output.h"
#include "verbose.h"

using std::vector;
using std::map;
using std::string;
using std::ifstream;
using std::ofstream;
using std::ostringstream;
using namespace pnapi;

/// the command line parameters
gengetopt_args_info args_info;

/// a suffix for the output filename
string suffix = "";

/// a variable holding the time of the call
clock_t start_clock = clock();

typedef enum { TYPE_OPENNET, TYPE_LOLANET, TYPE_PNMLNET, TYPE_WOFLANNET } objectType;

struct FileObject {
    objectType type;
    string filename;

    PetriNet* net;

    FileObject(string f) : filename(f), net(NULL) {}
};

/// check if a file exists and can be opened for reading
inline bool fileExists(const std::string& filename) {
    std::ifstream tmp(filename.c_str(), std::ios_base::in);
    return tmp.good();
}

/// evaluate the command line parameters
void evaluateParameters(int argc, char** argv) {
    // initialize the parameters structure
    struct cmdline_parser_params* params = cmdline_parser_params_create();

    // call the cmdline parser
    if (cmdline_parser(argc, argv, &args_info) != 0) {
        abort(1, "invalid command-line parameter(s)");
    }

    // read a configuration file if necessary
    if (args_info.config_given) {
        // initialize the config file parser
        params->initialize = 0;
        params->override = 0;

        // call the config file parser
        if (cmdline_parser_config_file(args_info.config_arg, &args_info, params) != 0) {
            abort(14, "error reading configuration file '%s'", args_info.config_arg);
        } else {
            status("using configuration file '%s'", _cfilename_(args_info.config_arg));
        }
    } else {
        // check for configuration files
        std::string conf_generic_filename = "petri.conf";
        std::string conf_filename = fileExists(conf_generic_filename) ? conf_generic_filename :
                                    (fileExists(std::string(SYSCONFDIR) + "/" + conf_generic_filename) ?
                                     (std::string(SYSCONFDIR) + "/" + conf_generic_filename) : "");

        if (conf_filename != "") {
            // initialize the config file parser
            params->initialize = 0;
            params->override = 0;
            if (cmdline_parser_config_file(const_cast<char*>(conf_filename.c_str()), &args_info, params) != 0) {
                abort(14, "error reading configuration file '%s'", conf_filename.c_str());
            } else {
                status("using configuration file '%s'", _cfilename_(conf_filename));
            }
        } else {
            status("not using a configuration file");
        }
    }

    free(params);
}


/// a function collecting calls to organize termination (close files, ...)
void terminationHandler() {
    // print statistics
    if (args_info.stats_flag) {
        message("runtime: %.2f sec", (static_cast<double>(clock()) - static_cast<double>(start_clock)) / CLOCKS_PER_SEC);
        fprintf(stderr, "%s%s%s: memory consumption: ", _cm_, PACKAGE, _c_);
        int doNotCare = system((std::string("ps -o rss -o comm | ") + TOOL_GREP + " " + PACKAGE + " | " + TOOL_AWK + " '{ if ($1 > max) max = $1 } END { print max \" KB\" }' 1>&2").c_str());
        doNotCare = doNotCare; // get rid of compiler warning
    }
}


int main(int argc, char** argv) {
    // set the function to call on normal termination
    atexit(terminationHandler);

    evaluateParameters(argc, argv);

    vector<FileObject> objects;

    // store invocation in a string for meta information in file output
    string invocation;
    for (int i = 0; i < argc; ++i) {
        invocation += string(argv[i]) + " ";
    }

    try {

        // set configured tools
        PetriNet::setGenet(args_info.genet_arg);
        PetriNet::setPetrify(args_info.petrify_arg);

        PetriNet::AutomatonConverter aConverter = (args_info.petrify_given ? PetriNet::PETRIFY :
                                                   (args_info.genet_given ? PetriNet::GENET : PetriNet::STATEMACHINE));

        /********
        * INPUT *
        ********/
        if (!args_info.inputs_num) {
            // read from stdin
            FileObject current("stdin");

            // try to parse net
            switch (args_info.input_arg) {
                case(input_arg_owfn): {
                    current.net = new PetriNet();
                    std::cin >> meta(io::INPUTFILE, current.filename)
                             >> meta(io::CREATOR, PACKAGE_STRING)
                             >> meta(io::INVOCATION, invocation) >> io::owfn >> *(current.net);

                    current.type = TYPE_OPENNET;
                    break;
                }
                case(input_arg_lola): {
                    current.net = new PetriNet();
                    std::cin >> meta(io::INPUTFILE, current.filename)
                             >> meta(io::CREATOR, PACKAGE_STRING)
                             >> meta(io::INVOCATION, invocation) >> io::lola >> *(current.net);

                    current.type = TYPE_LOLANET;
                    break;
                }
                case(input_arg_pnml): {
                    current.net = new PetriNet();
                    std::cin >> meta(io::INPUTFILE, current.filename)
                             >> meta(io::CREATOR, PACKAGE_STRING)
                             >> meta(io::INVOCATION, invocation) >> io::pnml >> *(current.net);

                    current.type = TYPE_PNMLNET;
                    break;
                }
                case(input_arg_sa): {
                    Automaton sa;
                    std::cin >> meta(io::INPUTFILE, current.filename)
                             >> meta(io::CREATOR, PACKAGE_STRING)
                             >> meta(io::INVOCATION, invocation) >> io::sa >> sa;
                    current.net = new PetriNet(sa, aConverter);

                    current.type = TYPE_OPENNET;
                    break;
                }
                case(input_arg_tpn): {
                    current.net = new PetriNet();
                    std::cin >> meta(io::INPUTFILE, current.filename)
                             >> meta(io::CREATOR, PACKAGE_STRING)
                             >> meta(io::INVOCATION, invocation) >> io::woflan >> *(current.net);

                    current.type = TYPE_WOFLANNET;
                    break;
                }
            }
            if (args_info.canonicalNames_given) {
                map<string, string> names = current.net->canonicalNames();

                if (args_info.canonicalNames_arg != NULL) {
                    // try to open file to write
                    Output outfile(args_info.canonicalNames_arg, "names output");

                    PNAPI_FOREACH(n, names) {
                        outfile.stream() << (n->first) << ": " << (n->second) << std::endl;
                    }
                }
            }

            std::stringstream ss;
            ss << io::stat << *(current.net);

            status("<stdin>: %s", ss.str().c_str());
            current.net->getInterface().setName("<stdin>");

            // store object
            objects.push_back(current);
        } else {
            // read from files
            for (unsigned int i = 0; i < args_info.inputs_num; ++i) {
                // prepare a new object
                FileObject current(args_info.inputs[i]);

                // try to open file
                ifstream infile(args_info.inputs[i], ifstream::in);
                if (!infile.is_open()) {
                    abort(3, "could not read from file '%s'", args_info.inputs[i]);
                }

                // try to parse net
                try {
                    switch (args_info.input_arg) {
                        case(input_arg_owfn): {
                            current.net = new PetriNet();
                            infile >> meta(io::INPUTFILE, current.filename)
                                   >> meta(io::CREATOR, PACKAGE_STRING)
                                   >> meta(io::INVOCATION, invocation) >> io::owfn >> *(current.net);

                            current.type = TYPE_OPENNET;
                            break;
                        }
                        case(input_arg_pnml): {
                            current.net = new PetriNet();
                            infile >> meta(io::INPUTFILE, current.filename)
                                   >> meta(io::CREATOR, PACKAGE_STRING)
                                   >> meta(io::INVOCATION, invocation) >> io::pnml >> *(current.net);

                            current.type = TYPE_OPENNET;
                            break;
                        }
                        case(input_arg_lola): {
                            current.net = new PetriNet();
                            infile >> meta(io::INPUTFILE, current.filename)
                                   >> meta(io::CREATOR, PACKAGE_STRING)
                                   >> meta(io::INVOCATION, invocation) >> io::lola >> *(current.net);

                            current.type = TYPE_LOLANET;
                            break;
                        }
                        case(input_arg_sa): {
                            Automaton sa;
                            infile >> meta(io::INPUTFILE, current.filename)
                                   >> meta(io::CREATOR, PACKAGE_STRING)
                                   >> meta(io::INVOCATION, invocation) >> io::sa >> sa;
                            current.net = new PetriNet(sa, aConverter);

                            current.type = TYPE_OPENNET;
                            break;
                        }
                        case(input_arg_tpn): {
                            current.net = new PetriNet();
                            infile >> meta(io::INPUTFILE, current.filename)
                                   >> meta(io::CREATOR, PACKAGE_STRING)
                                   >> meta(io::INVOCATION, invocation) >> io::woflan >> *(current.net);

                            current.type = TYPE_WOFLANNET;
                            break;
                        }
                    }
                    if (args_info.canonicalNames_given) {
                        map<string, string> names = current.net->canonicalNames();

                        if (args_info.canonicalNames_arg != NULL) {
                            // try to open file to write
                            Output outfile(args_info.canonicalNames_arg, "names output");

                            PNAPI_FOREACH(n, names) {
                                outfile.stream() << (n->first) << ": " << (n->second) << std::endl;
                            }
                        }
                    }//TODO
                } catch (exception::InputError& error) {
                    infile.close();
                    throw error;
                }

                infile.close();

                std::stringstream ss;
                ss << io::stat << *(current.net);
                status("%s: %s", args_info.inputs[i], ss.str().c_str());
                current.net->getInterface().setName(args_info.inputs[i]);

                // store object
                objects.push_back(current);
            }
        }


        /***************
         * COMPOSITION *
         ***************/
        if (args_info.compose_given) {
            // try to open file
            ifstream infile(args_info.compose_arg, ifstream::in);
            if (!infile.is_open()) {
                abort(3, "could not read from file '%s'", args_info.compose_arg);
            }

            PetriNet secondNet; // to store composition "partner"
            string secondNetName = args_info.compose_arg;

            // read net
            infile >> meta(io::INPUTFILE, secondNetName)
                   >> meta(io::CREATOR, PACKAGE_STRING)
                   >> meta(io::INVOCATION, invocation) >> io::owfn >> secondNet;

            secondNet.getInterface().setName(secondNetName);

            // compose nets
            for (size_t i = 0; i < objects.size(); ++i) {
                objects[i].net->compose(secondNet, objects[i].filename, secondNetName);
                objects[i].filename += ".composed";
            }
        }


        /***********
         * PRODUCT *
         ***********/
        if (args_info.produce_given) {
            if (args_info.inputs_num > 1) {
                abort(4, "at most one net can be used with '--produce' parameter");
            }

            // try to open file
            ifstream infile(args_info.produce_arg, ifstream::in);
            if (!infile.is_open()) {
                abort(3, "could not read from file '%s'", args_info.produce_arg);
            }

            PetriNet constraintNet; // to store constraint

            // read net
            infile >> meta(io::INPUTFILE, args_info.produce_arg)
                   >> meta(io::CREATOR, PACKAGE_STRING)
                   >> meta(io::INVOCATION, invocation) >> io::owfn >> constraintNet;

            // produce nets
            objects[0].net->produce(constraintNet);
        }


        /****************
        * MODIFICATIONS *
        *****************/
        if (args_info.normalize_given) {
            suffix += ".normalized";
            for (unsigned int i = 0; i < objects.size(); ++i) {
                status("normalizing reducing Petri net '%s'...", objects[i].filename.c_str());

                objects[i].net->normalize();
            }
        }

        if (args_info.negate_given) {
            suffix += ".negated";
            for (unsigned int i = 0; i < objects.size(); ++i) {
                status("negating the final condition of net '%s'...", objects[i].filename.c_str());

                objects[i].net->getFinalCondition().negate();
            }
        }

        if (args_info.mirror_given) {
            suffix += ".mirrored";
            for (unsigned int i = 0; i < objects.size(); ++i) {
                status("mirroring the net '%s'...", objects[i].filename.c_str());

                objects[i].net->mirror();
            }
        }

        if (args_info.dnf_given) {
            suffix += ".dnf";
            for (unsigned int i = 0; i < objects.size(); ++i) {
                status("calculation dnf of final condition of net '%s'...", objects[i].filename.c_str());

                objects[i].net->getFinalCondition().dnf();
            }
        }

        if (args_info.guessFormula_given) {
            for (unsigned int i = 0; i < objects.size(); ++i) {
                Place* sink = NULL;
                PNAPI_FOREACH(p, objects[i].net->getPlaces()) {
                    if ((*p)->getPostset().empty()) {
                        if (sink != NULL) {
                            abort(7, "net has more than one sink place: '%s' and '%s'", sink->getName().c_str(), (*p)->getName().c_str());
                        }
                        sink = *p;
                    }
                }

                if (sink == NULL) {
                    abort(7, "did not find a sink place");
                }

                status("sink place is '%s'", sink->getName().c_str());
                objects[i].net->getFinalCondition() = (*sink == 1);
                objects[i].net->getFinalCondition().allOtherPlacesEmpty(*objects[i].net);
            }
        }


        /***********************
        * STRUCTURAL REDUCTION *
        ***********************/
        if (args_info.reduce_given) {
            suffix += ".reduced";
            for (unsigned int i = 0; i < objects.size(); ++i) {
                PetriNet::ReductionLevel level = PetriNet::NONE;

                // collect the chosen reduction rules
                for (unsigned int j = 0; j < args_info.reduce_given; ++j) {
                    PetriNet::ReductionLevel newLevel = PetriNet::NONE;

                    switch (args_info.reduce_arg[j]) {
                        case(reduce_arg_1):
                            newLevel = PetriNet::LEVEL_1;
                            break;
                        case(reduce_arg_2):
                            newLevel = PetriNet::LEVEL_2;
                            break;
                        case(reduce_arg_3):
                            newLevel = PetriNet::LEVEL_3;
                            break;
                        case(reduce_arg_4):
                            newLevel = PetriNet::LEVEL_4;
                            break;
                        case(reduce_arg_5):
                            newLevel = PetriNet::LEVEL_5;
                            break;
                        case(reduce_arg_6):
                            newLevel = PetriNet::LEVEL_6;
                            break;
                        case(reduce_arg_starke):
                            newLevel = PetriNet::SET_STARKE;
                            break;
                        case(reduce_arg_pillat):
                            newLevel = PetriNet::SET_PILLAT;
                            break;
                        case(reduce_arg_dead_nodes):
                            newLevel = PetriNet::DEAD_NODES;
                            break;
                        case(reduce_arg_identical_places):
                            newLevel = PetriNet::IDENTICAL_PLACES;
                            break;
                        case(reduce_arg_identical_transitions):
                            newLevel = PetriNet::IDENTICAL_TRANSITIONS;
                            break;
                        case(reduce_arg_series_places):
                            newLevel = PetriNet::SERIES_PLACES;
                            break;
                        case(reduce_arg_series_transitions):
                            newLevel = PetriNet::SERIES_TRANSITIONS;
                            break;
                        case(reduce_arg_self_loop_places):
                            newLevel = PetriNet::SELF_LOOP_PLACES;
                            break;
                        case(reduce_arg_self_loop_transitions):
                            newLevel = PetriNet::SELF_LOOP_TRANSITIONS;
                            break;
                        case(reduce_arg_equal_places):
                            newLevel = PetriNet::EQUAL_PLACES;
                            break;
                        case(reduce_arg_starke3p):
                            newLevel = PetriNet::STARKE_RULE_3_PLACES;
                            break;
                        case(reduce_arg_starke3t):
                            newLevel = PetriNet::STARKE_RULE_3_TRANSITIONS;
                            break;
                        case(reduce_arg_starke4):
                            newLevel = PetriNet::STARKE_RULE_4;
                            break;
                        case(reduce_arg_starke5):
                            newLevel = PetriNet::STARKE_RULE_5;
                            break;
                        case(reduce_arg_starke6):
                            newLevel = PetriNet::STARKE_RULE_6;
                            break;
                        case(reduce_arg_starke7):
                            newLevel = PetriNet::STARKE_RULE_7;
                            break;
                        case(reduce_arg_starke8):
                            newLevel = PetriNet::STARKE_RULE_8;
                            break;
                        case(reduce_arg_starke9):
                            newLevel = PetriNet::STARKE_RULE_9;
                            break;
                        case(reduce_arg_once):
                            newLevel = PetriNet::ONCE;
                            break;
                        case(reduce_arg_k_boundedness):
                            newLevel = PetriNet::K_BOUNDEDNESS;
                            break;
                        case(reduce_arg_boundedness):
                            newLevel = PetriNet::BOUNDEDNESS;
                            break;
                        case(reduce_arg_liveness):
                            newLevel = PetriNet::LIVENESS;
                            break;
                        default: /* do nothing */
                            break;
                    }

                    level = (PetriNet::ReductionLevel)(level | newLevel);
                }

                status("structurally reducing Petri net '%s'...", objects[i].filename.c_str());

                objects[i].net->reduce(level);
            }
        }


        /************************
        * STRUCTURAL PROPERTIES *
        ************************/
        if (args_info.check_given or
                args_info.isFreeChoice_given or
                args_info.isNormal_given or
                args_info.isWorkflow_given) {
            for (unsigned int i = 0; i < objects.size(); ++i) {
                std::stringstream ss;

                // check for free choice
                if (args_info.check_arg == check_arg_freechoice or args_info.isFreeChoice_given) {
                    ss << objects[i].net->isFreeChoice() << std::endl;
                }

                // check for normality
                if (args_info.check_arg == check_arg_normal or args_info.isNormal_given) {
                    ss << objects[i].net->isNormal() << std::endl;
                }

                // check for workflow structure
                if (args_info.check_arg == check_arg_workflow or args_info.isWorkflow_given) {
                    ss << objects[i].net->isWorkflow() << std::endl;
                }

                message("%s: %s", objects[i].filename.c_str(), ss.str().c_str());
            }
        }


        /*********
        * OUTPUT *
        *********/
        if (args_info.output_given) {
            for (unsigned int i = 0; i < objects.size(); ++i) {
                for (unsigned int j = 0; j < args_info.output_given; ++j) {
                    // try to open file to write
                    string outname = objects[i].filename + suffix + "." + args_info.output_orig[j];
                    Output outfile(outname, std::string(args_info.output_orig[j]) +  " output");

                    outfile.stream() << meta(io::CREATOR, std::string(PACKAGE_STRING) + " Frontend (" + CONFIG_BUILDSYSTEM + ")");
                    outfile.stream() << meta(io::OUTPUTFILE, outname);

                    if (args_info.removePorts_flag) {
                        outfile.stream() << io::removePorts;
                    }

                    switch (args_info.output_arg[j]) {

                            // create oWFN output
                        case(output_arg_owfn): {
                            outfile.stream() << io::owfn << *(objects[i].net);
                            break;
                        }

                        // create LoLA output
                        case(output_arg_lola): {
                            if (args_info.formula_flag) {
                                outfile.stream() << io::lola << io::formula << *(objects[i].net);
                            } else {
                                outfile.stream() << io::lola << *(objects[i].net);
                            }
                            break;
                        }

                        // create PNML output
                        case(output_arg_pnml): {
                            outfile.stream() << io::pnml << *(objects[i].net);
                            break;
                        }

                        // create automaton output
                        case(output_arg_sa): {
                            Automaton sauto(*(objects[i].net));
                            outfile.stream() << io::sa << sauto;
                            break;
                        }

                        // create Woflan output
                        case(output_arg_tpn): {
                            outfile.stream() << io::woflan << *(objects[i].net);
                            break;
                        }

                        // create dot output
                        case(output_arg_dot): {
                            outfile.stream() << io::dot << *(objects[i].net);
                            break;
                        }

                        // create output using Graphviz dot
                        case(output_arg_png):
                        case(output_arg_eps):
                        case(output_arg_pdf):
                        case(output_arg_svg): {
#if !defined(HAVE_POPEN) or !defined(HAVE_PCLOSE)
                            abort(6, "petri: cannot open UNIX pipe to Graphviz dot");
#endif
                            ostringstream d;
                            if (args_info.removePorts_flag) {
                                d << io::removePorts;
                            }
                            d << io::dot << *(objects[i].net);
                            string call = string(args_info.dot_arg) + " -T" + args_info.output_orig[j] + " -q -o " + outname;
                            FILE* s = popen(call.c_str(), "w");
                            assert(s);
                            fprintf(s, "%s\n", d.str().c_str());
                            assert(!ferror(s));
                            pclose(s);
                            break;
                        }
                    }
                }
            }
        }
    } catch (exception::InputError& error) {
        std::stringstream ss;
        ss << error;
        abort(2, "Input Error: %s", ss.str().c_str());
    } catch (exception::Error& error) {
        std::stringstream ss;
        ss << error;
        abort(7, "Exception caught: %s", ss.str().c_str());
    }

    /**********
    * CLEANUP *
    **********/
    for (unsigned int i = 0; i < objects.size(); ++i) {
        delete(objects[i].net);
    }

    cmdline_parser_free(&args_info);

    return EXIT_SUCCESS;
}
