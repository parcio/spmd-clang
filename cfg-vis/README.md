# CFG-Visualization

German:
(English Version below.)

Dieses Unterprojekt beinhaltet die Darstellung des Kontrollflusses eines C-Programms als interaktiven Graphen in einer Browseranwendung. 

Voraussetzungen für die Nutzung der Kontrollflussgraphvisualisierung ist die Installation des Clang Compilers zzgl. des SPMD-Plugins (siehe ../spmd-plugin) sowie des Paketmanagers [npm](https://www.npmjs.com/). Außerdem muss der Quellcode des Programms, das man visualisieren möchte, vorhanden sein. 

Bei erstmaliger Nutzung der Visualisierungsanwendung müssen die erforderlichen JavaScript-Bibliotheken installiert werden. Dafür folgendes im Anwendungsverzeichnis (in dem auch die `package.json`-Datei liegt) ausgeführt werden: 
``` npm install ```

Bei wiederholter Nutzung sind diese 5 Schritte notwendig, um einen Kontrollflusgraphen darzustellen:

1. Das darzustellende Programm muss mittels des Clang-Compilers inkl. des Plugins kompiliert werden, um eine JSON-Ausgabedatei zu erzeugen. Im Arbeitsverzeichnis (wo das C-Programm liegt):
```MPICH_CC=clang mpicc -g -lm -fplugin=$SPMD_PLUGIN <programm-name>.c ```

2. Die Visualisierungsanwendung wird auf einem [Vite-Entwicklungsserver](https://de.vite.dev/) gestartet, indem im Anwendungsverzeichnis folgender Befehl ausgeführt wird:
```npm run dev ```

3. Die Anwendung wird (standardmäßig) unter dem Port 4200 im Browser aufgerufen.

4. Über den "Add Graphdata"-Button in der Navigationsleiste kann der Dateiexplorer geöffnet und die während des Kompiliervorgangs erstellte JSO-Datei ausgewählt werden. Daraufhin werden die Graphdaten in die Anwendung geladen. 

5. In der Navigationsleiste links ist nun der erstellte Kontrollflussgraph auswählbar. 



English:

This subproject contains the visualization of the control flow of a C-program as an interactive graph in a web application.

Prerequisites for using the control flow graph visualization are the installation of the Clang compiler including the SPMD-plugin (see ../spmd-plugin) as well as the package manager [npm](https://www.npmjs.com/). Additionally, the source code of the program to be visualized must be available.

When using the visualization application for the first time, the required JavaScript libraries must be installed. To do this, execute the following in the application directory (where the `package.json` file is located):
``` npm install ```

For subsequent use, these 5 steps are necessary to display a control flow graph:

1. The program to be visualized must be compiled using the Clang compiler including the plugin to generate a JSON output file. In the working directory (where the C-program is located):
```MPICH_CC=clang mpicc -g -lm -fplugin=$SPMD_PLUGIN <program-name>.c ```

2. The visualization application is started on a [Vite development server](https://vitejs.dev/) by executing the following command in the application directory:
```npm run dev ```

3. The application is started (by default) on port 4200 in your browser.

4. Using the "Add Graphdata"-button in the navigation bar, the file explorer can be opened and the JSON file created during the compilation process can be selected. After that, the graph data is loaded into the application.

5. In the left navigation bar, the created control flow graph can now be selected. 