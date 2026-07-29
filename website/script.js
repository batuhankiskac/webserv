// Webserv Test JavaScript
console.log('Webserv test page loaded');

// Test HTTP methods
async function testMethod(method) {
    const resultEl = document.getElementById('method-result');
    resultEl.textContent = `Testing ${method}...\n`;
    
    try {
        const response = await fetch('/', {
            method: method,
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            }
        });
        const text = await response.text();
        resultEl.textContent += `${method}: ${response.status} ${response.statusText}\n`;
        if (text.length > 200) {
            resultEl.textContent += text.substring(0, 200) + '...';
        } else {
            resultEl.textContent += text;
        }
    } catch (err) {
        resultEl.textContent += `${method}: Error - ${err.message}`;
    }
}

// DELETE file upload
document.getElementById('delete-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const filename = document.getElementById('delete-filename').value.trim();
    const resultEl = document.getElementById('delete-result');
    
    if (!filename) return;
    
    resultEl.innerHTML = 'Deleting...';
    
    try {
        const response = await fetch(`/upload/${filename}`, {
            method: 'DELETE'
        });
        resultEl.innerHTML = `DELETE ${filename}: ${response.status} ${response.statusText}`;
    } catch (err) {
        resultEl.innerHTML = `Error: ${err.message}`;
    }
});

// Auto-refresh upload directory after upload
const uploadForms = document.querySelectorAll('form[action="/upload"], form[action="/small-upload"]');
uploadForms.forEach(form => {
    form.addEventListener('submit', () => {
        setTimeout(() => {
            fetch('/upload/')
                .then(r => r.text())
                .then(html => {
                    console.log('Upload directory refreshed');
                });
        }, 500);
    });
});