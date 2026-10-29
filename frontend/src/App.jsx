import { useState, useEffect } from 'react';

export default function App() {
  const [maps, setMaps] = useState({ firstFit: [], bestFit: [], worstFit: [] });
  const [val, setVal] = useState(180);
  const [unit, setUnit] = useState("KB");

  const fetchMemoryMap = async () => {
    try {
      const response = await fetch('https://cma-frontend-sadi.onrender.com/api/memory-map');
      const data = await response.json();
      setMaps(data);
    } catch (error) {
      console.error("Failed to fetch memory map:", error);
    }
  };

  useEffect(() => {
    let cancelled = false;

    const loadMemoryMap = async () => {
      try {
        const response = await fetch('https://cma-frontend-sadi.onrender.com/api/memory-map');
        const data = await response.json();
        if (!cancelled) setMaps(data);
      } catch (error) {
        console.error("Failed to fetch memory map:", error);
      }
    };

    loadMemoryMap();
    return () => { cancelled = true; };
  }, []);

  const handleAllocate = async () => {
    try {
      await fetch('https://cma-frontend-sadi.onrender.com/api/allocate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ val: parseFloat(val), unit: unit })
      });
      fetchMemoryMap();
    } catch (error) {
      console.error("Allocation failed:", error);
    }
  };

  const handleDeallocate = async (id) => {
    try {
      await fetch('https://cma-frontend-sadi.onrender.com/api/deallocate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: id })
      });
      fetchMemoryMap();
    } catch (error) {
      console.error("Deallocation failed:", error);
    }
  };

  const renderMemoryGrid = (title, blocks) => (
    <div style={{ marginBottom: '50px', width: '100%', maxWidth: '1100px', margin: '0 auto 50px auto' }}>
      <h3 style={{ textAlign: 'center', color: '#334155', letterSpacing: '1.5px', marginBottom: '20px', textTransform: 'uppercase', fontSize: '14px', fontWeight: '700' }}>
        {title}
      </h3>
      
      <div style={{ 
        display: 'flex', 
        width: '100%', 
        height: '100px', 
        backgroundColor: '#f1f5f9', 
        boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05)',
        borderRadius: '10px',
        border: '1px solid #cbd5e1',
        overflow: 'hidden'
      }}>
        {blocks.map((block, index) => (
          <div 
            key={index}
            style={{
              flexGrow: block.sizeBytes, 
              minWidth: block.status === 'USED' ? '70px' : '40px', // Prevents button/text squishing
              background: block.status === 'FREE' 
                ? 'linear-gradient(135deg, #10b981, #059669)' // Emerald gradient
                : 'linear-gradient(135deg, #6366f1, #4f46e5)', // Indigo gradient
              borderRight: '1px solid rgba(255,255,255,0.3)',
              display: 'flex', 
              flexDirection: 'column',
              alignItems: 'center', 
              justifyContent: 'center',
              fontSize: '12px',
              color: '#ffffff',
              transition: 'all 0.4s cubic-bezier(0.4, 0, 0.2, 1)',
              padding: '6px',
              gap: '4px' // Cleanly separates text and buttons without absolute positioning
            }}
          >
            <strong style={{ opacity: 0.9, fontSize: '11px', letterSpacing: '1px' }}>
              {block.status}
            </strong> 
            
            <span style={{ fontWeight: '600', fontSize: '13px' }}>
              {Number(block.displaySize).toFixed(2)} {block.unit}
            </span>
            
            {block.status === 'USED' && (
              <button 
                onClick={() => handleDeallocate(block.id)} 
                style={{
                  backgroundColor: 'rgba(255, 255, 255, 0.2)',
                  border: '1px solid rgba(255,255,255,0.4)',
                  borderRadius: '6px',
                  fontSize: '11px',
                  padding: '4px 12px',
                  cursor: 'pointer',
                  fontWeight: '600',
                  color: '#ffffff',
                  transition: 'all 0.2s ease',
                  marginTop: '2px',
                  boxShadow: '0 2px 4px rgba(0,0,0,0.1)'
                }}
                onMouseOver={(e) => {
                  e.target.style.backgroundColor = '#ffffff';
                  e.target.style.color = '#4f46e5';
                }}
                onMouseOut={(e) => {
                  e.target.style.backgroundColor = 'rgba(255, 255, 255, 0.2)';
                  e.target.style.color = '#ffffff';
                }}
              >
                Free
              </button>
            )}
          </div>
        ))}
      </div>
    </div>
  );

  return (
    <div style={{ 
      minHeight: '100vh', 
      backgroundColor: '#f8fafc', 
      padding: '50px 20px', 
      fontFamily: '"Inter", "Segoe UI", Roboto, sans-serif'
    }}>
      
      <h1 style={{ textAlign: 'center', color: '#0f172a', fontSize: '2.5rem', fontWeight: '800', marginBottom: '40px', letterSpacing: '-0.5px' }}>
        Memory Allocation Visualizer
      </h1>
      
      <div style={{ 
        backgroundColor: '#ffffff', 
        padding: '25px 30px', 
        borderRadius: '12px', 
        maxWidth: '600px', 
        margin: '0 auto 60px auto',
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        gap: '15px',
        boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.05), 0 10px 10px -5px rgba(0, 0, 0, 0.02)',
        border: '1px solid #f1f5f9'
      }}>
        <label style={{ fontWeight: '600', color: '#64748b', fontSize: '1.1rem' }}>Request Size:</label>
        <input 
          type="number" 
          value={val} 
          onChange={(e) => setVal(e.target.value)} 
          style={{ 
            padding: '10px 15px', 
            borderRadius: '8px', 
            border: '2px solid #e2e8f0',
            fontSize: '1.1rem',
            width: '130px',
            color: '#ffffff',
            fontWeight: '600',
            outline: 'none',
            transition: 'border-color 0.2s'
          }} 
          onFocus={(e) => e.target.style.borderColor = '#6366f1'}
          onBlur={(e) => e.target.style.borderColor = '#e2e8f0'}
        />
        
        <select 
          value={unit} 
          onChange={(e) => setUnit(e.target.value)} 
          style={{ 
            padding: '10px', 
            borderRadius: '8px', 
            border: '2px solid #e2e8f0',
            fontSize: '1.1rem',
            color: '#fff',
            fontWeight: '600',
            cursor: 'pointer',
            outline: 'none'
          }}
        >
          <option value="B">B</option>
          <option value="KB">KB</option>
          <option value="MB">MB</option>
        </select>
        
        <button 
          onClick={handleAllocate} 
          style={{ 
            padding: '12px 24px', 
            backgroundColor: '#0f172a', 
            color: 'white', 
            border: 'none', 
            borderRadius: '8px',
            fontSize: '1.1rem',
            fontWeight: '600',
            cursor: 'pointer',
            transition: 'transform 0.1s, background-color 0.2s',
            boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.1)'
          }}
          onMouseOver={(e) => e.target.style.backgroundColor = '#1e293b'}
          onMouseOut={(e) => e.target.style.backgroundColor = '#0f172a'}
          onMouseDown={(e) => e.target.style.transform = 'scale(0.97)'}
          onMouseUp={(e) => e.target.style.transform = 'scale(1)'}
        >
          Allocate
        </button>
      </div>

      {renderMemoryGrid("First-Fit Allocation", maps.firstFit || [])}
      {renderMemoryGrid("Best-Fit Allocation", maps.bestFit || [])}
      {renderMemoryGrid("Worst-Fit Allocation", maps.worstFit || [])}
      
    </div>
  );
}