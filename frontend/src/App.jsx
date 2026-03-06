import React, { useState, useEffect } from 'react';

function App() {
  const [formData, setFormData] = useState({ title: '', description: '', priority_code: 1, role_code: 1 });
  const [tasks, setTasks] = useState([]);
  const [loading, setLoading] = useState(false);
  const [actualHoursInput, setActualHoursInput] = useState({});

  // Завантаження задач при старті
  const fetchTasks = async () => {
    try {
      const res = await fetch('/api/tasks');
      if (res.ok) {
        const data = await res.json();
        setTasks(data.tasks);
      }
    } catch (e) { console.error("Помилка завантаження задач"); }
  };

  useEffect(() => { fetchTasks(); }, []);

  const handleInputChange = (e) => {
    const { name, value } = e.target;
    setFormData({ ...formData, [name]: name.includes('code') ? parseInt(value, 10) : value });
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setLoading(true);
    try {
      await fetch('/api/task', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(formData)
      });
      setFormData({ title: '', description: '', priority_code: 1, role_code: 1 });
      fetchTasks(); // Оновлюємо список
    } finally { setLoading(false); }
  };

  // Функція для збереження фактичного часу (Continuous Training trigger)
  const handleResolve = async (taskId) => {
    const hours = actualHoursInput[taskId];
    if (!hours) return alert("Введіть фактичні години!");

    try {
      await fetch(`/api/task/${taskId}/resolve`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ actual_hours: parseFloat(hours) })
      });
      fetchTasks(); // Оновлюємо список
      alert("Дані збережено! Тепер їх можна використати для донавчання моделі.");
    } catch (e) { console.error(e); }
  };

  return (
    <div style={{ display: 'flex', gap: '40px', padding: '20px', fontFamily: 'sans-serif' }}>
      {/* Ліва колонка: Форма створення */}
      <div style={{ flex: 1, maxWidth: '400px' }}>
        <h2>Створити задачу</h2>
        <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '15px' }}>
          <input type="text" name="title" value={formData.title} onChange={handleInputChange} placeholder="Назва задачі" required />
          <textarea name="description" value={formData.description} onChange={handleInputChange} placeholder="Опис" required rows="4" />
          <select name="priority_code" value={formData.priority_code} onChange={handleInputChange}>
            <option value={0}>Low</option><option value={1}>Medium</option><option value={2}>High</option><option value={3}>Critical</option>
          </select>
          <select name="role_code" value={formData.role_code} onChange={handleInputChange}>
            <option value={0}>Junior</option><option value={1}>Middle</option><option value={2}>Senior</option>
          </select>
          <button type="submit" disabled={loading}>{loading ? 'Обробка...' : 'Створити та Оцінити ML'}</button>
        </form>
      </div>

      {/* Права колонка: Беклог */}
      <div style={{ flex: 2 }}>
        <h2>Беклог задач</h2>
        <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left' }}>
          <thead>
            <tr style={{ borderBottom: '2px solid #ccc' }}>
              <th>ID</th><th>Назва</th><th>ML Прогноз</th><th>Фактично (Години)</th><th>Дія</th>
            </tr>
          </thead>
          <tbody>
            {tasks.map(t => (
              <tr key={t.id} style={{ borderBottom: '1px solid #eee' }}>
                <td>{t.id}</td>
                <td>{t.title}</td>
                <td><strong>{t.ml_prediction?.toFixed(2)} год</strong></td>
                <td>
                  {t.actual_hours !== -1.0 ? (
                     <span>{t.actual_hours} год (Завершено)</span>
                  ) : (
                    <input 
                      type="number" step="0.1" 
                      placeholder="Реальний час"
                      onChange={(e) => setActualHoursInput({...actualHoursInput, [t.id]: e.target.value})}
                      style={{ width: '100px' }}
                    />
                  )}
                </td>
                <td>
                  {t.actual_hours === -1.0 && (
                    <button onClick={() => handleResolve(t.id)}>Зберегти факт</button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default App;