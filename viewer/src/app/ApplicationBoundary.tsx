import {Component,type ReactNode} from 'react';
import {errorMessage} from '../data/errors';
export class ApplicationBoundary extends Component<{children:ReactNode},{error:string}>{
  state={error:''};
  static getDerivedStateFromError(error:unknown){return {error:errorMessage(error)};}
  render(){if(!this.state.error)return this.props.children;return <div className="application"><main className="empty-view"><h1>Não foi possível continuar a visualização</h1><p role="alert">{this.state.error}</p><button onClick={()=>{localStorage.removeItem('navier-view-settings');location.reload();}}>Reabrir com configurações padrão</button></main></div>;}
}
