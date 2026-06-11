import { callable } from '@steambrew/webkit';

const receiveFrontendMethod = callable<[{ message: string; status: boolean; count: number }], boolean>('test_frontend_message_callback');
const frontendMethod = callable<[{ country: string; age: number }], string>('frontend:classname.method');

export default async function WebkitMain() {
	const success = await receiveFrontendMethod({
		message: 'Hello World From Frontend!',
		status: true,
		count: 69,
	});
	console.log('Result from receiveFrontendMethod:', success);

	const result = await frontendMethod({
		country: 'USA',
		age: 25,
	});
	console.log('Result from frontendMethod:', result);
}
