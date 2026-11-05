const menuButton = document.querySelector('.menu-toggle');
const navLinks = document.querySelector('.nav-links');

menuButton.addEventListener('click', () => {
  const isOpen = navLinks.classList.toggle('open');
  menuButton.setAttribute('aria-expanded', String(isOpen));
});

document.querySelectorAll('.nav-links a').forEach(link => {
  link.addEventListener('click', () => {
    navLinks.classList.remove('open');
    menuButton.setAttribute('aria-expanded', 'false');
  });
});

document.getElementById('year').textContent = new Date().getFullYear();

document.getElementById('contactForm').addEventListener('submit', event => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  const email = 'info.visavistaconsultants@gmail.com'; // Replace with your real business email
  const subject = encodeURIComponent(`Visa Vista enquiry - ${form.get('service')}`);
  const body = encodeURIComponent(
    `Name: ${form.get('name')}\nEmail or phone: ${form.get('contact')}\nService: ${form.get('service')}\nMessage: ${form.get('message') || 'Not provided'}`
  );
  window.location.href = `mailto:${email}?subject=${subject}&body=${body}`;
});
