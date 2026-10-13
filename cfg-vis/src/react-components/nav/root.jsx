import { useEffect } from "react";
import { Outlet, Link, useLoaderData, Form, NavLink, useNavigation } from "react-router-dom";
import useStore  from "../../store";
import { useShallow } from "zustand/react/shallow";

import FileSearcher from "./FileHandler.jsx";

const selector = (state) => ({
  cfgFiles: state.cfgFiles
});

export default function Root() {

    const { cfgFiles } = useStore(useShallow(selector));
    const navigation = useNavigation();
    
    return (
      <>
        <div id="sidebar">
          <h1>Control Flow Graphs</h1>
          <FileSearcher />
          <nav>
            { cfgFiles.length ? (
              <ul>
                { cfgFiles.map((graph) => (
                  <li key={graph.graphId}>
                    <NavLink to={`graphs/${graph.graphId}`}
                      className={({ isActive, isPending }) =>
                        isActive ? "active" : isPending ? "pending" : ""
                      }
                    >
                      <b>{graph.graphId}</b> {graph.fileName}
                    </NavLink>
                  </li>
                ))}
              </ul>
            ) : (
              <p>
                <i>No Data loaded</i>
              </p>
            )}
          </nav>
        </div>
            <div id="detail" className={ navigation.state === "loading" ? "loading" : ""}>
            <Outlet />
        </div> 
      </>
    );
  }