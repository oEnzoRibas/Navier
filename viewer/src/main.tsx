import {createRoot} from 'react-dom/client';
import {App} from './app/App';
import {ApplicationBoundary} from './app/ApplicationBoundary';
import './styles/application.css';
const container=document.getElementById('root');if(!container)throw new Error('Application root missing');
createRoot(container).render(<ApplicationBoundary><App/></ApplicationBoundary>);

