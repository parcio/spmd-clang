import { useNavigate, useRouteError } from "react-router-dom";

export default function ErrorPage() {
  const error = useRouteError();
  console.error(error);

  let navigate = useNavigate();

  function handleClick() {
    navigate("/");
  }

  return (
    <div id="error-page">
      <h1>Oops!</h1>
      <p>Sorry, an unexpected error has occurred.</p>
      <p>
        <i>{error.statusText || error.message}</i>
      </p>
      <button type="button" onClick={handleClick}>
           Zurück zum Anfang
      </button>
    </div>
  );
}