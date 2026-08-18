import React from "react";

import { ValdiWebRenderer } from 'valdi_gpt_npm/src/web_renderer/src/ValdiWebRenderer';

import { ValdiGptApp } from 'valdi_gpt_npm/src/valdi_gpt/src/ValdiGptApp';


class ValdiGPTDemo extends React.Component {
  constructor(props) {
    super(props);
    this.elementRef = React.createRef();
  }

  componentDidMount() {
    if (this.elementRef.current) {
      console.log('DIV mounted:', this.elementRef.current);
      const webRenderer = new ValdiWebRenderer(this.elementRef.current);
      webRenderer.renderRootComponent(
        ValdiGptApp,
        {},
        {},
        {},
      );
    }
  }

  render() {
    return (
      <div>
        <h2>A placeholder React component for development</h2>
        <div ref={this.elementRef}></div>
      </div>
    );
  }
}

const App = () => {
  return <ValdiGPTDemo />;
};

export default App;
