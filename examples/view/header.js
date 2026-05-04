import { view, text } from 'kwikui';
export function Header(props) {
    return view({ height: 60, background: "#333" }, [
        text({ text: props.title, color: "#fff", fontSize: 24 })
    ]);
}