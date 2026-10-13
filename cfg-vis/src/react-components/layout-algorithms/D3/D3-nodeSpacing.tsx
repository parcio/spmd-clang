import React, { useEffect } from "react";
import { useState } from "react";
import { Box, Stack, Slider } from "@mui/material";
import {
  forceLink,
  forceManyBody,
  forceSimulation,
  SimulationLinkDatum,
  SimulationNodeDatum,
} from "d3-force";
import useStore, { AppState } from "../../../store";
import { useParams } from "react-router-dom";
import { useShallow } from "zustand/react/shallow";
import * as d3 from "d3";
import collide from "./collide";

type SimNodeType = SimulationNodeDatum & Node;

const selector = (state: AppState) => ({
  graphData: state.graphData,
  setNodes: state.setNodes,
  getGroupEdges: state.getGroupEdges,
  mergeNodes: state.mergeNodes,
});


function useForceLayout(strength, useCollisionForce) {
  const { graphId } = useParams();
  const { graphData, setNodes, getGroupEdges, mergeNodes } = useStore(
    useShallow(selector)
  );
  let graph = graphData[Number(graphId)];

  useEffect(() => {
    if(strength == 0) {
      return;
    }
    let nodes = graph.groups[graph.groups.length - 1].map((node) => {
      return graph.nodes[node.index];
    });
    let edges = getGroupEdges(Number(graphId), nodes, graph.edges);

    const simulationNodes: SimNodeType[] = nodes.map((node) => {
      let w = node.width ?? 0;
      let h = node.height ?? 0;
      let x = node.position.x ?? 0;
      let y = node.position.y ?? 0;
      return {
        ...node,
        x: x + (w/2),
        y: y + (h/2),
      };
    });

    simulationNodes.filter(nd => !Number.isNaN(nd.x) && !Number.isNaN(nd.y))

    const simulationLinks: SimulationLinkDatum<SimNodeType>[] = edges.map(
      (edge) => {
        return { ...edge };
      }
    );

    if (!simulationNodes.length || !simulationLinks.length) {
      return;
    }

    console.log(simulationNodes)

    const simulation = forceSimulation()
      .nodes(simulationNodes)
      .force("charge", forceManyBody().strength(strength))
      .on("tick", () => {
        let layoutedNodes = nodes.map((node, i) => {
          const { x, y } = simulationNodes[i];
          let w = node.width ?? 0;
          let h = node.height ?? 0;
          let _x = x ?? 0;
          let _y = y ?? 0;
          return { ...node, position: { x: _x - w/2, y: _y - h/2 } };
        });
        setNodes(Number(graphId), mergeNodes(Number(graphId), layoutedNodes));
      });

    if (useCollisionForce) {
      console.log("collision applied")
      simulation
      .alphaTarget(0.3) // stay hot
      .velocityDecay(0.1) // low friction
      .force(
        "collide", collide()
      )
      //.force("x", d3.forceX(1000).strength(0.001))
      //.force("y", d3.forceY(1000).strength(0.001));
    }

    return () => {
      simulation.stop();
    };
  }, [strength]);
}

const marks = [
  {
    value: -1000,
    label: "locker",
  },
  {
    value: -500,
    label: "-500",
  },
  {
    value: 0,
    label: "0",
  },
  {
    value: 500,
    label: "500",
  },
  {
    value: 1000,
    label: "dicht",
  },
];

function valuetext(value: number) {
  return `${value}°C`;
}

export default function NodeSpacing() {
  const [strength, setStrength] = useState<number>(0);

  const handleChange = (event: Event, newStrength: number | number[]) => {
    if (nodeSpacing) setStrength(newStrength as number);
  };

  const [nodeSpacing, setValue] = useState(false); // set NodeSpacing acitve/inactive
  const [useCollisionForce, setUseCollisionForce] = useState(true);

  useForceLayout(strength, useCollisionForce);

  const handleChangeCollision = () => {
    setUseCollisionForce(!useCollisionForce);
  };

  return (
    <div>
      <button
        style={{
          marginTop: "5" + "pt",
          marginBottom: "5" + "pt",
          width: 300 + "px",
        }}
        onClick={() => {
          setStrength(0);
          if(nodeSpacing) {
            setUseCollisionForce(false);
          }
          setValue(!nodeSpacing);
        }}
      >
        Mehrkörperkräfte {nodeSpacing ? "aktiviert" : "deaktiviert"}
      </button>
      <div>
        <input
          type="checkbox"
          checked={useCollisionForce}
          onChange={handleChangeCollision}
        />
        <label style={{ marginLeft: 5 + "px", fontSize: 12 + "pt" }}>
          zzgl. Kollisionskraft
        </label>
      </div>
      <Box sx={{ width: 295 }}>
        <Stack spacing={2} direction="row" sx={{ alignItems: "center", mt: 2 }}>
          {/*<VolumeDown />*/}
          <Slider
            aria-label="strength"
            defaultValue={-1000}
            value={strength}
            onChange={handleChange}
            getAriaValueText={valuetext}
            valueLabelDisplay="auto"
            marks={marks}
            min={-1000}
            max={+1000}
          />
          {/*<VolumeUp />*/}
        </Stack>
      </Box>
    </div>
  );
}
