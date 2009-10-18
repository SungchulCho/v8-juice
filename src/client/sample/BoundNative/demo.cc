#if defined(NDEBUG)
#undef NDEBUG
#endif
#include <cassert>
// Copyright 2009 Stephan Beal
// (http://wanderinghorse.net/home/stephan). All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// END OF LICENSE
// This code is derived from the v8 shell application.
/************************************************************************

 ************************************************************************/


#include <v8.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>


#include "MyNative.h"
#include <v8/juice/PathFinder.h>
//#include <v8/juice/ToSource.h>
#include <v8/juice/JuiceShell.h>
#include <v8/juice/ClassBinder.h>

//namespace bind = ::v8::juice::bind;

v8::Handle<v8::Value> Quit(const v8::Arguments& args);

using namespace ::v8;
using namespace ::v8::juice;


v8::juice::JuiceShell * ShellInstance = 0; 
int main(int argc, char * argv[])
{
    //v8::V8::SetFlagsFromCommandLine(&argc, argv, false);
    {
        v8::HandleScope handle_scope;
        v8::juice::cleanup::CleanupSentry cleaner;
        v8::juice::JuiceShell shell("v8juice");
        ShellInstance = &shell;
        shell.ProcessMainArgv(argc,argv,1);
        shell.SetupJuiceEnvironment();
        MyNative::SetupClass(shell.Context());
        bool run_shell = (argc == 1);
        v8::Locker tlocker;
        v8::TryCatch jtry;
        std::string const endofargs("--");
        for (int i = 1; i < argc; i++) {
            const char* str = argv[i];
            if( endofargs == str )
            {
                break;
            }
            else if (strncmp(str, "--", 2) == 0)
            {
                printf("Error: unknown flag %s.\n", str);
                return 2;
            } else if (strcmp(str, "-e") == 0 && (i + 1 < argc)) {
                // Execute argument given to -e option directly
                v8::HandleScope handle_scope;
                std::string source(argv[i + 1] ? argv[i + 1] : "");
                if (!shell.ExecuteString(source, "[-e script]", 0, &jtry))
                {
                    return 1;
                }
                ++i;
            } else {
                // Use all other arguments as names of files to load and run.
                shell.Include( str, false, &jtry );
                if( jtry.HasCaught() )
                {
                    std::cerr << "Exception while including ["<<str<<"]: "
                              << convert::JSToStdString(jtry.Exception())
                              <<'\n';
                    return 1;
                }
                continue;
            }
        }
        if (run_shell)
        {
            shell.AddGlobalFunc( "quit", Quit );
            v8::TryCatch jtry;
            shell.InputLoop( v8::juice::JuiceShell::StdinLineFetcher, &std::cout, &jtry );
            std::cout << std::endl;
        }
    }
    return 0;
}

// The callback that is invoked by v8 whenever the JavaScript 'quit'
// function is called.  Quits.
v8::Handle<v8::Value> Quit(const v8::Arguments& args) {
  // If not arguments are given args[0] will yield undefined which
  // converts to the integer value 0.
  int exit_code = args[0]->Int32Value();
  exit(exit_code);
  return v8::Undefined();
}

