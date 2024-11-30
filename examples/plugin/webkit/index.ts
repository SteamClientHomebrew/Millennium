import { callable } from "@steambrew/webkit"

const receive_frontend_message = callable<[{ message: string, status: boolean, count: number }]>("Backend.receive_frontend_message")

export default async function WebkitMain () {
    const message = await receive_frontend_message({
        message: "Hello World From Frontend!",
        status: true,
        count: 69
    })

    console.log("Result from callServerMethod:", message)
}