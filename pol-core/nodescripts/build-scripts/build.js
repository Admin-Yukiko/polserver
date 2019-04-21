// @ts-check

const generator = require("./wrap-generator");

async function run() {
  const outPath = process.argv.slice(2).join(" ") || process.cwd();
  
  await generator.generateModules(outPath);
  await generator.generateObjects(outPath);
}

if (require.main === module) {
  run().catch(e => { 
    console.error("Error",e); 
    process.exit(1);
  });
}
