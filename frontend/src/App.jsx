import React, { useState, useEffect, useRef } from 'react';

export default function App() {
  const [maps, setMaps] = useState({ firstFit: [], bestFit: [], worstFit: [] });
  const [analytics, setAnalytics] = useState({ firstFit: null, bestFit: null, worstFit: null });
  const [val, setVal] = useState(180);
  const [unit, setUnit] = useState("KB");
  
  const [logs, setLogs] = useState([]);
  const logContainerRef = useRef(null);

  const addLog = (message, type = 'info') => {
    const timestamp = new Date().toLocaleTimeString('en-US', { hour12: false });
    setLogs(prev => [...prev, { time: timestamp, text: message, type }]);
  };

  useEffect(() => {
    if (logContainerRef.current) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [logs]);

  const fetchData = async () => {
    try {
      const [mapRes, analyticsRes] = await Promise.all([
        // Pass the current unit state to the backend
        fetch(`https://cma-frontend-1.onrender.com/api/memory-map?unit=${unit}`),
        fetch('https://cma-frontend-1.onrender.com/api/analytics')
      ]);
      setMaps(await mapRes.json());
      setAnalytics(await analyticsRes.json());
    } catch (error) {
      console.error("Failed to fetch data:", error);
      addLog("ERROR: Failed to sync with C++ Backend.", "error");
    }
  };

  // Update your existing useEffect to listen for changes to the 'unit' state
  useEffect(() => { 
    fetchData(); 
  }, [unit]);
  
  // (Keep your initialization log in a separate effect so it only fires once)
  useEffect(() => {
    addLog("System Initialized. Connected to C++ Memory Engine.", "info");
  }, []);

  const handleAllocate = async () => {
    try {
      addLog(`Requesting allocation: ${val} ${unit}...`, "info");
      const response = await fetch('https://cma-frontend-1.onrender.com/api/allocate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ val: parseFloat(val), unit: unit })
      });
      const data = await response.json();
      
      if (data.status === "SUCCESS") {
        addLog(`ALLOCATE SUCCESS: Safely carved ${val} ${unit} from the arena.`, "success");
      } else {
        addLog(`ALLOCATE FAILED: Insufficient contiguous space for ${val} ${unit}.`, "error");
      }
      fetchData();
    } catch (error) {
      addLog(`ERROR: Allocation failed to execute.`, "error");
    }
  };

  const handleDeallocate = async (id) => {
    try {
      addLog(`Initiating deallocation for Block ID: ${id}...`, "info");
      await fetch('https://cma-frontend-1.onrender.com/api/deallocate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: id })
      });
      addLog(`FREE SUCCESS: Block ${id} deallocated. Triggered bidirectional coalescing.`, "success");
      fetchData();
    } catch (error) {
      addLog(`ERROR: Deallocation for Block ${id} failed.`, "error");
    }
  };

  const AnalyticsPanel = ({ stats }) => {
    if (!stats) return null;
    return (
      <div style={{ display: 'flex', justifyContent: 'space-around', marginBottom: '15px', backgroundColor: '#ffffff', padding: '15px', borderRadius: '8px', boxShadow: '0 2px 4px rgba(0,0,0,0.05)', border: '1px solid #e2e8f0' }}>
        <div style={{ textAlign: 'center', flex: 1, borderRight: '1px solid #e2e8f0' }}>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: '#64748b', textTransform: 'uppercase' }}>External Fragmentation</div>
          <div style={{ fontSize: '20px', fontWeight: '900', color: '#f97316' }}>{stats.externalFrag.toFixed(2)}%</div>
        </div>
        <div style={{ textAlign: 'center', flex: 1 }}>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: '#64748b', textTransform: 'uppercase' }}>Active Blocks</div>
          <div style={{ fontSize: '20px', fontWeight: '900', color: '#3b82f6' }}>{stats.totalBlocks}</div>
        </div>
      </div>
    );
  };

  const MemoryGrid = ({ title, blocks, stats }) => (
    <div style={{ marginBottom: '50px', width: '100%', maxWidth: '1100px', margin: '0 auto 50px auto' }}>
      <h3 style={{ textAlign: 'center', color: '#334155', letterSpacing: '1.5px', marginBottom: '15px', textTransform: 'uppercase', fontSize: '14px', fontWeight: '700' }}>
        {title}
      </h3>
      
      <AnalyticsPanel stats={stats} />
      
      <div style={{ display: 'flex', width: '100%', height: '100px', backgroundColor: '#f1f5f9', boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1)', borderRadius: '10px', border: '1px solid #cbd5e1', overflow: 'hidden' }}>
        {blocks.map((block, index) => (
          <div 
            key={index}
            style={{
              flexGrow: block.sizeBytes, 
              minWidth: block.status === 'USED' ? '70px' : '40px',
              background: block.status === 'FREE' ? 'linear-gradient(135deg, #10b981, #059669)' : 'linear-gradient(135deg, #6366f1, #4f46e5)',
              borderRight: '1px solid rgba(255,255,255,0.3)',
              display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
              fontSize: '12px', color: '#ffffff', transition: 'all 0.4s ease', padding: '6px', gap: '4px'
            }}
          >
            <strong style={{ opacity: 0.9, fontSize: '11px', letterSpacing: '1px' }}>{block.status}</strong> 
            <span style={{ fontWeight: '600', fontSize: '13px' }}>{Number(block.displaySize).toFixed(2)} {block.unit}</span>
            {block.status === 'USED' && (
              <button onClick={() => handleDeallocate(block.id)} style={{ backgroundColor: 'rgba(255, 255, 255, 0.2)', border: '1px solid rgba(255,255,255,0.4)', borderRadius: '6px', fontSize: '11px', padding: '4px 12px', cursor: 'pointer', fontWeight: '600', color: '#ffffff', transition: 'all 0.2s ease', marginTop: '2px', boxShadow: '0 2px 4px rgba(0,0,0,0.1)' }}>Free</button>
            )}
          </div>
        ))}
      </div>
    </div>
  );

  return (
    <div style={{ minHeight: '100vh', backgroundColor: '#f8fafc', padding: '50px 20px', fontFamily: '"Inter", "Segoe UI", Roboto, sans-serif' }}>
      <h1 style={{ textAlign: 'center', color: '#0f172a', fontSize: '2.5rem', fontWeight: '800', marginBottom: '40px', letterSpacing: '-0.5px' }}>Memory Allocation Visualizer</h1>
      
      <div style={{ backgroundColor: '#ffffff', padding: '25px 30px', borderRadius: '12px', maxWidth: '600px', margin: '0 auto 60px auto', display: 'flex', justifyContent: 'center', alignItems: 'center', gap: '15px', boxShadow: '0 20px 25px -5px rgba(0, 0, 0, 0.05)', border: '1px solid #f1f5f9' }}>
        <label style={{ fontWeight: '600', color: '#64748b', fontSize: '1.1rem' }}>Request Size:</label>
        <input type="number" value={val} onChange={(e) => setVal(e.target.value)} style={{ padding: '10px 15px', borderRadius: '8px', border: '2px solid #e2e8f0', fontSize: '1.1rem', width: '110px', color: '#ffffff', fontWeight: '600', outline: 'none' }} />
        <select value={unit} onChange={(e) => setUnit(e.target.value)} style={{ padding: '10px', borderRadius: '8px', border: '2px solid #e2e8f0', fontSize: '1.1rem', color: '#ffffff', fontWeight: '600', cursor: 'pointer', outline: 'none' }}>
          <option value="B">B</option><option value="KB">KB</option><option value="MB">MB</option>
        </select>
        <button onClick={handleAllocate} style={{ padding: '12px 24px', backgroundColor: '#0f172a', color: 'white', border: 'none', borderRadius: '8px', fontSize: '1.1rem', fontWeight: '600', cursor: 'pointer' }}>Allocate</button>
      </div>

      <MemoryGrid title="First-Fit Allocation" blocks={maps.firstFit || []} stats={analytics.firstFit} />
      <MemoryGrid title="Best-Fit Allocation" blocks={maps.bestFit || []} stats={analytics.bestFit} />
      <MemoryGrid title="Worst-Fit Allocation" blocks={maps.worstFit || []} stats={analytics.worstFit} />

      <div style={{ maxWidth: '1100px', margin: '0 auto', backgroundColor: '#0f172a', borderRadius: '8px', padding: '20px', boxShadow: '0 10px 15px -3px rgba(0, 0, 0, 0.1)', border: '1px solid #1e293b' }}>
        <h3 style={{ color: '#94a3b8', fontSize: '14px', textTransform: 'uppercase', letterSpacing: '1px', marginBottom: '15px', fontWeight: '600', borderBottom: '1px solid #1e293b', paddingBottom: '10px' }}>
          System Audit Log
        </h3>
        
        <div ref={logContainerRef} style={{ height: '180px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '8px', fontFamily: '"Fira Code", "Courier New", monospace', fontSize: '13px' }}>
          {logs.map((log, index) => {
            let textColor = '#e2e8f0'; 
            if (log.type === 'success') textColor = '#4ade80';
            if (log.type === 'error') textColor = '#f87171';
            
            return (
              <div key={index} style={{ color: textColor, lineHeight: '1.4' }}>
                <span style={{ color: '#64748b', marginRight: '10px' }}>[{log.time}]</span> 
                {log.text}
              </div>
            );
          })}
        </div>
      </div>
      
    </div>
  );
}