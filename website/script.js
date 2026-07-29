async function testMethod(method) {
    const result = document.getElementById('method-result');
    result.textContent = `Testing ${method}…`;

    try {
        const response = await fetch('/', { method });
        const body = await response.text();
        result.textContent = `${method}: ${response.status} ${response.statusText}\n${body.slice(0, 300)}`;
    } catch (error) {
        result.textContent = `${method}: ${error.message}`;
    }
}

document.querySelectorAll('[data-method]').forEach((button) => {
    button.addEventListener('click', () => testMethod(button.dataset.method));
});

const deleteForm = document.getElementById('delete-form');
deleteForm.addEventListener('submit', async (event) => {
    event.preventDefault();

    const filename = document.getElementById('delete-filename').value.trim();
    const result = document.getElementById('delete-result');
    if (!filename) {
        return;
    }

    try {
        const response = await fetch(`/upload/${encodeURIComponent(filename)}`, {
            method: 'DELETE'
        });
        result.textContent = `DELETE ${filename}: ${response.status} ${response.statusText}`;
    } catch (error) {
        result.textContent = `DELETE ${filename}: ${error.message}`;
    }
});
