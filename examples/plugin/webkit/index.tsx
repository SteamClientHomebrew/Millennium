import { callable, Millennium } from '@steambrew/webkit';

// preact is a lightweight alternative to react that is compatible with react components
// This allows you to render components on pages without huge react dependencies
import { render } from 'preact';

const receiveFrontendMethod = callable<[{ message: string, status: boolean, count: number }], boolean>("Backend.receive_frontend_message")

interface ExampleComponentProps {
	success: boolean;
}

const ExampleComponent: preact.FunctionalComponent<ExampleComponentProps> = ({ success }): preact.VNode => (
	<div>
		<h1>Hello, World!</h1>
		<p>We're using preact to render react-like components on non-react pages.</p>
		<p>Message from backend: {success ? "Success!" : "Failure"}</p>
	</div>
);

type WebkitSettings = {
	BasicSettings: {
		overrideWebkitDocument: boolean,
		frontendCount: number
	}
}

export default async function WebkitMain () {
	const settings = await Millennium.getSettings<WebkitSettings>();

	const success = await receiveFrontendMethod({
		message: "Hello World From Frontend!",
		status: true,
		count: settings.BasicSettings.frontendCount,
	})

	console.log("Result from receive_frontend_message:", success)
	if (settings.BasicSettings.overrideWebkitDocument) {
		render(<ExampleComponent success={success}/>, document.body);
	}
}
